// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_stream_handler.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "chromeos/services/assistant/platform/audio_media_data_source.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

AudioStreamHandler::AudioStreamHandler(
    service_manager::Connector* connector,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : connector_(connector),
      task_runner_(task_runner),
      client_binding_(this),
      weak_factory_(this) {}

AudioStreamHandler::~AudioStreamHandler() = default;

void AudioStreamHandler::StartAudioDecoder(
    assistant_client::AudioOutput::Delegate* delegate,
    InitCB on_inited) {
  mojom::AssistantAudioDecoderClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  connector_->BindInterface(mojom::kAudioDecoderServiceName,
                            mojo::MakeRequest(&audio_decoder_factory_ptr_));

  mojom::AssistantMediaDataSourcePtr data_source;
  media_data_source_ =
      std::make_unique<AudioMediaDataSource>(&data_source, task_runner_);

  audio_decoder_factory_ptr_->CreateAssistantAudioDecoder(
      mojo::MakeRequest(&audio_decoder_), std::move(client),
      std::move(data_source));

  delegate_ = delegate;
  media_data_source_->set_delegate(delegate_);
  start_device_owner_on_main_thread_ = std::move(on_inited);
  audio_decoder_->OpenDecoder(base::BindOnce(
      &AudioStreamHandler::OnDecoderInitialized, weak_factory_.GetWeakPtr()));
}

void AudioStreamHandler::OnDecoderInitialized(bool success,
                                              int bytes_per_sample,
                                              int samples_per_second,
                                              int channels) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioStreamHandler::OnDecoderInitializedOnThread,
                     weak_factory_.GetWeakPtr(), success, bytes_per_sample,
                     samples_per_second, channels));
}

void AudioStreamHandler::OnNewBuffers(
    const std::vector<std::vector<uint8_t>>& buffers) {
  if (buffers.size() == 0)
    no_more_data_ = true;

  for (const auto& buffer : buffers)
    decoded_data_.emplace_back(buffer);

  is_decoding_ = false;
  FillDecodedBuffer(buffer_to_copy_, size_to_copy_);
}

// TODO(wutao): Needs to pass |playback_timestamp| to LibAssistant.
void AudioStreamHandler::FillBuffer(
    void* buffer,
    int buffer_size,
    int64_t playback_timestamp,
    assistant_client::Callback1<int> on_filled) {
  DCHECK(!on_filled_);

  on_filled_ = std::move(on_filled);
  buffer_to_copy_ = buffer;
  size_to_copy_ = buffer_size;

  FillDecodedBuffer(buffer, buffer_size);
}

void AudioStreamHandler::OnEndOfStream() {
  if (delegate_)
    delegate_->OnEndOfStream();
}

void AudioStreamHandler::OnError(assistant_client::AudioOutput::Error error) {
  if (delegate_)
    delegate_->OnError(error);
}

void AudioStreamHandler::OnStopped() {
  delegate_->OnStopped();
  media_data_source_->set_delegate(nullptr);
  delegate_ = nullptr;
}

void AudioStreamHandler::OnDecoderInitializedOnThread(bool success,
                                                      int bytes_per_sample,
                                                      int samples_per_second,
                                                      int channels) {
  if (!success) {
    OnError(assistant_client::AudioOutput::Error::FATAL_ERROR);
    std::move(start_device_owner_on_main_thread_);
    return;
  }

  DCHECK(bytes_per_sample == 2 || bytes_per_sample == 4);
  const assistant_client::OutputStreamFormat format = {
      bytes_per_sample == 2
          ? assistant_client::OutputStreamEncoding::STREAM_PCM_S16
          : assistant_client::OutputStreamEncoding::STREAM_PCM_S32,
      /*pcm_sample_rate=*/samples_per_second,
      /*pcm_num_channels=*/channels};
  if (!device_owner_started_) {
    DCHECK(start_device_owner_on_main_thread_);
    DCHECK(!on_filled_);
    std::move(start_device_owner_on_main_thread_).Run(format);
    device_owner_started_ = true;
  }
}

void AudioStreamHandler::FillDecodedBuffer(void* buffer, int buffer_size) {
  if (on_filled_ && (decoded_data_.size() > 0 || no_more_data_)) {
    int size_copied = 0;
    // Fill buffer with data not more than requested.
    while (!decoded_data_.empty() && size_copied < buffer_size) {
      std::vector<uint8_t>& data = decoded_data_.front();
      int audio_buffer_size = static_cast<int>(data.size());
      if (size_copied + audio_buffer_size > buffer_size)
        audio_buffer_size = buffer_size - size_copied;

      memcpy(reinterpret_cast<uint8_t*>(buffer) + size_copied, data.data(),
             audio_buffer_size);
      size_copied += audio_buffer_size;
      if (audio_buffer_size < static_cast<int>(data.size()))
        data.erase(data.begin(), data.begin() + audio_buffer_size);
      else
        decoded_data_.pop_front();
    }

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioStreamHandler::OnFillBufferOnThread,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(on_filled_), size_copied));
  }

  if (decoded_data_.empty() && !no_more_data_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioStreamHandler::DecodeOnThread,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioStreamHandler::OnFillBufferOnThread(
    assistant_client::Callback1<int> on_filled,
    int num_bytes) {
  std::move(on_filled)(num_bytes);
}

void AudioStreamHandler::DecodeOnThread() {
  if (is_decoding_)
    return;

  is_decoding_ = true;
  audio_decoder_->Decode();
}

}  // namespace assistant
}  // namespace chromeos
