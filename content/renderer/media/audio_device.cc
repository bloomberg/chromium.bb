// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_device.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_util.h"

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
  filter_ = RenderThreadImpl::current()->audio_message_filter();
  audio_data_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    float* channel_data = new float[buffer_size];
    audio_data_.push_back(channel_data);
  }
}

AudioDevice::~AudioDevice() {
  // The current design requires that the user calls Stop() before deleting
  // this class.
  CHECK_EQ(0, stream_id_);
  for (int i = 0; i < channels_; ++i)
    delete [] audio_data_[i];
}

void AudioDevice::Start() {
  AudioParameters params;
  params.format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
  params.channels = channels_;
  params.sample_rate = static_cast<int>(sample_rate_);
  params.bits_per_sample = bits_per_sample_;
  params.samples_per_packet = buffer_size_;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::InitializeOnIOThread, this, params));
}

bool AudioDevice::Stop() {
  // Max waiting time for Stop() to complete. If this time limit is passed,
  // we will stop waiting and return false. It ensures that Stop() can't block
  // the calling thread forever.
  const base::TimeDelta kMaxTimeOut = base::TimeDelta::FromMilliseconds(1000);

  base::WaitableEvent completion(false, false);

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::ShutDownOnIOThread, this, &completion));

  // We wait here for the IO task to be completed to remove race conflicts
  // with OnLowLatencyCreated() and to ensure that Stop() acts as a synchronous
  // function call.
  if (completion.TimedWait(kMaxTimeOut)) {
    if (audio_thread_.get()) {
      socket_->Close();
      audio_thread_->Join();
      audio_thread_.reset(NULL);
    }
  } else {
    LOG(ERROR) << "Failed to shut down audio output on IO thread";
    return false;
  }

  return true;
}

bool AudioDevice::SetVolume(double volume) {
  if (volume < 0 || volume > 1.0)
    return false;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::SetVolumeOnIOThread, this, volume));

  volume_ = volume;

  return true;
}

void AudioDevice::GetVolume(double* volume) {
  // Return a locally cached version of the current scaling factor.
  *volume = volume_;
}

void AudioDevice::InitializeOnIOThread(const AudioParameters& params) {
  // Make sure we don't call Start() more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return;

  stream_id_ = filter_->AddDelegate(this);
  Send(new AudioHostMsg_CreateStream(stream_id_, params, true));
}

void AudioDevice::StartOnIOThread() {
  if (stream_id_)
    Send(new AudioHostMsg_PlayStream(stream_id_));
}

void AudioDevice::ShutDownOnIOThread(base::WaitableEvent* completion) {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_) {
    completion->Signal();
    return;
  }

  filter_->RemoveDelegate(stream_id_);
  Send(new AudioHostMsg_CloseStream(stream_id_));
  stream_id_ = 0;

  completion->Signal();
}

void AudioDevice::SetVolumeOnIOThread(double volume) {
  if (stream_id_)
    Send(new AudioHostMsg_SetVolume(stream_id_, volume));
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
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK(length);

  // Takes care of the case when Stop() is called before OnLowLatencyCreated().
  if (!stream_id_) {
    base::SharedMemory::CloseHandle(handle);
    // Close the socket handler.
    base::SyncSocket socket(socket_handle);
    return;
  }

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);

  DCHECK_GE(length, buffer_size_ * sizeof(int16) * channels_);

  socket_.reset(new base::SyncSocket(socket_handle));
  // Allow the client to pre-populate the buffer.
  FireRenderCallback();

  audio_thread_.reset(
      new base::DelegateSimpleThread(this, "renderer_audio_thread"));
  audio_thread_->Start();

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::StartOnIOThread, this));
}

void AudioDevice::OnVolume(double volume) {
  NOTIMPLEMENTED();
}

void AudioDevice::Send(IPC::Message* message) {
  filter_->Send(message);
}

// Our audio thread runs here.
void AudioDevice::Run() {
  audio_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  int pending_data;
  const int samples_per_ms = static_cast<int>(sample_rate_) / 1000;
  const int bytes_per_ms = channels_ * (bits_per_sample_ / 8) * samples_per_ms;

  while ((sizeof(pending_data) == socket_->Receive(&pending_data,
                                                   sizeof(pending_data))) &&
         (pending_data >= 0)) {
    // Convert the number of pending bytes in the render buffer
    // into milliseconds.
    audio_delay_milliseconds_ = pending_data / bytes_per_ms;
    FireRenderCallback();
  }
}

void AudioDevice::FireRenderCallback() {
  TRACE_EVENT0("audio", "AudioDevice::FireRenderCallback");

  if (callback_) {
    // Update the audio-delay measurement then ask client to render audio.
    callback_->Render(audio_data_, buffer_size_, audio_delay_milliseconds_);

    // Interleave, scale, and clip to int16.
    media::InterleaveFloatToInt16(audio_data_,
                                  static_cast<int16*>(shared_memory_data()),
                                  buffer_size_);
  }
}
