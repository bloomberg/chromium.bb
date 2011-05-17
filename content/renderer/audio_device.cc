// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/audio_device.h"

#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "content/common/audio_messages.h"
#include "content/common/child_process.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
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

}  // namespace

AudioDevice::AudioDevice(size_t buffer_size,
                         int channels,
                         double sample_rate,
                         RenderCallback* callback)
    : buffer_size_(buffer_size),
      channels_(channels),
      bits_per_sample_(16),
      sample_rate_(sample_rate),
      callback_(callback),
      audio_delay_milliseconds_(0),
      volume_(1.0),
      stream_id_(0) {
  audio_data_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    float* channel_data = new float[buffer_size];
    audio_data_.push_back(channel_data);
  }
  // Lazily create the message filter and share across AudioDevice instances.
  filter_ = AudioMessageFilterCreator::SharedFilter();
}

AudioDevice::~AudioDevice() {
  // Make sure we have been shut down.
  DCHECK_EQ(0, stream_id_);
  Stop();
  for (int i = 0; i < channels_; ++i)
    delete [] audio_data_[i];
}

bool AudioDevice::Start() {
  // Make sure we don't call Start() more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return false;

  AudioParameters params;
  params.format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
  params.channels = channels_;
  params.sample_rate = static_cast<int>(sample_rate_);
  params.bits_per_sample = bits_per_sample_;
  params.samples_per_packet = buffer_size_;

  // Ensure that the initialization task is posted on the I/O thread by
  // accessing the I/O message loop directly. This approach avoids a race
  // condition which could exist if the message loop of the filter was
  // used instead.
  DCHECK(ChildProcess::current()) << "Must be in the renderer";
  MessageLoop* message_loop = ChildProcess::current()->io_message_loop();
  if (!message_loop)
    return false;

  message_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioDevice::InitializeOnIOThread, params));

  return true;
}

bool AudioDevice::Stop() {
  if (!stream_id_)
    return false;

  filter_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioDevice::ShutDownOnIOThread));

  if (audio_thread_.get()) {
    socket_->Close();
    audio_thread_->Join();
  }

  return true;
}

bool AudioDevice::SetVolume(double volume) {
  if (!stream_id_)
    return false;

  if (volume < 0 || volume > 1.0)
    return false;

  filter_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioDevice::SetVolumeOnIOThread, volume));

  volume_ = volume;

  return true;
}

bool AudioDevice::GetVolume(double* volume) {
  if (!stream_id_)
    return false;

  // Return a locally cached version of the current scaling factor.
  *volume = volume_;

  return true;
}

void AudioDevice::InitializeOnIOThread(const AudioParameters& params) {
  stream_id_ = filter_->AddDelegate(this);
  filter_->Send(new AudioHostMsg_CreateStream(0, stream_id_, params, true));
}

void AudioDevice::StartOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioHostMsg_PlayStream(0, stream_id_));
}

void AudioDevice::ShutDownOnIOThread() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_)
    return;

  filter_->Send(new AudioHostMsg_CloseStream(0, stream_id_));
  filter_->RemoveDelegate(stream_id_);
  stream_id_ = 0;
}

void AudioDevice::SetVolumeOnIOThread(double volume) {
  if (stream_id_)
    filter_->Send(new AudioHostMsg_SetVolume(0, stream_id_, volume));
}

void AudioDevice::OnRequestPacket(AudioBuffersState buffers_state) {
  // This method does not apply to the low-latency system.
  NOTIMPLEMENTED();
}

void AudioDevice::OnStateChanged(AudioStreamState state) {
  if (state == kAudioStreamError) {
    DLOG(WARNING) << "AudioDevice::OnStateChanged(kError)";
  }
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

  // TODO(crogers) : check that length is big enough for buffer_size_

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);

  socket_.reset(new base::SyncSocket(socket_handle));
  // Allow the client to pre-populate the buffer.
  FireRenderCallback();

  audio_thread_.reset(
      new base::DelegateSimpleThread(this, "renderer_audio_thread"));
  audio_thread_->Start();

  if (filter_) {
    filter_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AudioDevice::StartOnIOThread));
  }
}

void AudioDevice::OnVolume(double volume) {
  NOTIMPLEMENTED();
}

// Our audio thread runs here.
void AudioDevice::Run() {
    audio_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  int pending_data;
  const int samples_per_ms = static_cast<int>(sample_rate_) / 1000;
  const int bytes_per_ms = channels_ * (bits_per_sample_ / 8) * samples_per_ms;

  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
                                                  pending_data >= 0) {
    {
      // Convert the number of pending bytes in the render buffer
      // into milliseconds.
      audio_delay_milliseconds_ = pending_data / bytes_per_ms;
    }

    FireRenderCallback();
  }
}

void AudioDevice::FireRenderCallback() {
  if (callback_) {
    // Update the audio-delay measurement then ask client to render audio.
    callback_->Render(audio_data_, buffer_size_, audio_delay_milliseconds_);

    // Interleave, scale, and clip to int16.
    int16* output_buffer16 = static_cast<int16*>(shared_memory_data());
    media::InterleaveFloatToInt16(audio_data_, output_buffer16, buffer_size_);
  }
}
