// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/audio_device.h"

#include "base/singleton.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_thread.h"
#include "media/audio/audio_util.h"

scoped_refptr<AudioMessageFilter> AudioDevice::filter_;

namespace {

// AudioMessageFilterCreator is intended to be used as a singleton so we can
// get access to a shared AudioMessageFilter.
// Example usage:
//   AudioMessageFilter* filter = AudioMessageFilterCreator::SharedFilter();

class AudioMessageFilterCreator {
 public:
  AudioMessageFilterCreator() {
    int routing_id;
    RenderThread::current()->Send(
        new ViewHostMsg_GenerateRoutingID(&routing_id));
    filter_ = new AudioMessageFilter(routing_id);
    RenderThread::current()->AddFilter(filter_);
  }

  static AudioMessageFilter* SharedFilter() {
    return GetInstance()->filter_.get();
  }

  static AudioMessageFilterCreator* GetInstance() {
    return Singleton<AudioMessageFilterCreator>::get();
  }

 private:
  scoped_refptr<AudioMessageFilter> filter_;
};

}

AudioDevice::AudioDevice(size_t buffer_size,
                         int channels,
                         double sample_rate,
                         RenderCallback* callback)
    : buffer_size_(buffer_size),
      channels_(channels),
      sample_rate_(sample_rate),
      callback_(callback),
      stream_id_(0) {
  audio_data_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    float* channel_data = new float[buffer_size];
    audio_data_.push_back(channel_data);
  }
}

AudioDevice::~AudioDevice() {
  Stop();
  for (int i = 0; i < channels_; ++i)
    delete [] audio_data_[i];
}

bool AudioDevice::Start() {
  // Make sure we don't call Start() more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return false;

  // Lazily create the message filter and share across AudioDevice instances.
  filter_ = AudioMessageFilterCreator::SharedFilter();

  stream_id_ = filter_->AddDelegate(this);

  ViewHostMsg_Audio_CreateStream_Params params;
  params.params.format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
  params.params.channels = channels_;
  params.params.sample_rate = static_cast<int>(sample_rate_);
  params.params.bits_per_sample = 16;
  params.params.samples_per_packet = buffer_size_;

  filter_->Send(
      new ViewHostMsg_CreateAudioStream(0, stream_id_, params, true));

  return true;
}

bool AudioDevice::Stop() {
  if (stream_id_) {
    OnDestroy();
    return true;
  }
  return false;
}

void AudioDevice::OnDestroy() {
  // Make sure we don't call destroy more than once.
  DCHECK_NE(0, stream_id_);
  if (!stream_id_)
    return;

  filter_->RemoveDelegate(stream_id_);
  filter_->Send(new ViewHostMsg_CloseAudioStream(0, stream_id_));
  stream_id_ = 0;
  if (audio_thread_.get()) {
    socket_->Close();
    audio_thread_->Join();
  }
}

void AudioDevice::OnRequestPacket(AudioBuffersState buffers_state) {
  // This method does not apply to the low-latency system.
  NOTIMPLEMENTED();
}

void AudioDevice::OnStateChanged(
    const ViewMsg_AudioStreamState_Params& state) {
  // Not needed in this simple implementation.
  NOTIMPLEMENTED();
}

void AudioDevice::OnCreated(
    base::SharedMemoryHandle handle, uint32 length) {
  // Not needed in this simple implementation.
  NOTIMPLEMENTED();
}

void AudioDevice::OnLowLatencyCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    uint32 length) {

#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK(length);
  DCHECK(!audio_thread_.get());

  // TODO(crogers) : check that length is big enough for buffer_size_

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);

  socket_.reset(new base::SyncSocket(socket_handle));
  // Allow the client to pre-populate the buffer.
  FireRenderCallback();

  // TODO(crogers): we could optionally set the thread to high-priority
  audio_thread_.reset(
      new base::DelegateSimpleThread(this, "renderer_audio_thread"));
  audio_thread_->Start();

  filter_->Send(new ViewHostMsg_PlayAudioStream(0, stream_id_));
}

void AudioDevice::OnVolume(double volume) {
  // Not needed in this simple implementation.
  NOTIMPLEMENTED();
}

// Our audio thread runs here.
void AudioDevice::Run() {
  int pending_data;
  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
                                                  pending_data >= 0) {
    FireRenderCallback();
  }
}

void AudioDevice::FireRenderCallback() {
  if (callback_) {
    // Ask client to render audio.
    callback_->Render(audio_data_, buffer_size_);

    // Interleave, scale, and clip to int16.
    int16* output_buffer16 = static_cast<int16*>(shared_memory_data());
    media::InterleaveFloatToInt16(audio_data_, output_buffer16, buffer_size_);
  }
}
