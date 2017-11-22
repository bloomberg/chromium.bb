// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mojo_audio_input_ipc.h"

#include <utility>

#include "base/bind_helpers.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace content {

MojoAudioInputIPC::MojoAudioInputIPC(StreamCreatorCB stream_creator)
    : stream_creator_(std::move(stream_creator)),
      client_binding_(this),
      weak_factory_(this) {}

MojoAudioInputIPC::~MojoAudioInputIPC() = default;

void MojoAudioInputIPC::CreateStream(media::AudioInputIPCDelegate* delegate,
                                     int session_id,
                                     const media::AudioParameters& params,
                                     bool automatic_gain_control,
                                     uint32_t total_segments) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  DCHECK(!delegate_);

  delegate_ = delegate;

  media::mojom::AudioInputStreamClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  client_binding_.set_connection_error_handler(base::BindOnce(
      &media::AudioInputIPCDelegate::OnError, base::Unretained(delegate_)));

  stream_creator_.Run(mojo::MakeRequest(&stream_), session_id, params,
                      automatic_gain_control, total_segments, std::move(client),
                      base::BindOnce(&MojoAudioInputIPC::StreamCreated,
                                     weak_factory_.GetWeakPtr()));

  // Unretained is safe since |delegate_| is required to remain valid until
  // CloseStream is called, which closes the binding.
  stream_.set_connection_error_handler(base::BindOnce(
      &media::AudioInputIPCDelegate::OnError, base::Unretained(delegate_)));
}

void MojoAudioInputIPC::RecordStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stream_.is_bound())
    stream_->Record();
}

void MojoAudioInputIPC::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stream_.is_bound())
    stream_->SetVolume(volume);
}

void MojoAudioInputIPC::CloseStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_binding_.Close();
  stream_.reset();
  delegate_ = nullptr;
}

void MojoAudioInputIPC::StreamCreated(
    mojo::ScopedSharedBufferHandle shared_memory,
    mojo::ScopedHandle socket,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  DCHECK(socket.is_valid());
  DCHECK(shared_memory.is_valid());

  base::PlatformFile socket_handle;
  auto result = mojo::UnwrapPlatformFile(std::move(socket), &socket_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);

  base::SharedMemoryHandle memory_handle;
  bool read_only = true;
  result = mojo::UnwrapSharedMemoryHandle(std::move(shared_memory),
                                          &memory_handle, nullptr, &read_only);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK(read_only);

  delegate_->OnStreamCreated(memory_handle, socket_handle, initially_muted);
}

void MojoAudioInputIPC::OnError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  delegate_->OnError();
}

void MojoAudioInputIPC::OnMutedStateChanged(bool is_muted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  delegate_->OnMuted(is_muted);
}

}  // namespace content
