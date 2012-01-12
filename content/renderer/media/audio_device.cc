// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_util.h"

AudioDevice::AudioDevice()
    : buffer_size_(0),
      channels_(0),
      bits_per_sample_(16),
      sample_rate_(0),
      latency_format_(AudioParameters::AUDIO_PCM_LOW_LATENCY),
      callback_(0),
      is_initialized_(false),
      audio_delay_milliseconds_(0),
      volume_(1.0),
      stream_id_(0),
      play_on_start_(true),
      is_started_(false),
      shared_memory_handle_(base::SharedMemory::NULLHandle()),
      memory_length_(0) {
  filter_ = RenderThreadImpl::current()->audio_message_filter();
}

AudioDevice::AudioDevice(size_t buffer_size,
                         int channels,
                         double sample_rate,
                         RenderCallback* callback)
    : bits_per_sample_(16),
      is_initialized_(false),
      audio_delay_milliseconds_(0),
      volume_(1.0),
      stream_id_(0),
      play_on_start_(true),
      is_started_(false),
      shared_memory_handle_(base::SharedMemory::NULLHandle()),
      memory_length_(0) {
  filter_ = RenderThreadImpl::current()->audio_message_filter();
  Initialize(buffer_size,
             channels,
             sample_rate,
             AudioParameters::AUDIO_PCM_LOW_LATENCY,
             callback);
}

void AudioDevice::Initialize(size_t buffer_size,
                             int channels,
                             double sample_rate,
                             AudioParameters::Format latency_format,
                             RenderCallback* callback) {
  CHECK_EQ(0, stream_id_) <<
      "AudioDevice::Initialize() must be called before Start()";

  CHECK(!is_initialized_);

  buffer_size_ = buffer_size;
  channels_ = channels;
  sample_rate_ = sample_rate;
  latency_format_ = latency_format;
  callback_ = callback;

  // Cleanup from any previous initialization.
  for (size_t i = 0; i < audio_data_.size(); ++i)
    delete [] audio_data_[i];

  audio_data_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    float* channel_data = new float[buffer_size];
    audio_data_.push_back(channel_data);
  }

  is_initialized_ = true;
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
  params.format = latency_format_;
  params.channels = channels_;
  params.sample_rate = static_cast<int>(sample_rate_);
  params.bits_per_sample = bits_per_sample_;
  params.samples_per_packet = buffer_size_;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::InitializeOnIOThread, this, params));
}

void AudioDevice::Stop() {
  DCHECK(MessageLoop::current() != ChildProcess::current()->io_message_loop());
  base::WaitableEvent completion(false, false);

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::ShutDownOnIOThread, this, &completion));

  // We wait here for the IO task to be completed to remove race conflicts
  // with OnLowLatencyCreated() and to ensure that Stop() acts as a synchronous
  // function call.
  completion.Wait();
  ShutDownAudioThread();
}

void AudioDevice::Play() {
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::PlayOnIOThread, this));
}

void AudioDevice::Pause(bool flush) {
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioDevice::PauseOnIOThread, this, flush));
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
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  // Make sure we don't create the stream more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return;

  stream_id_ = filter_->AddDelegate(this);
  Send(new AudioHostMsg_CreateStream(stream_id_, params, true));
}

void AudioDevice::PlayOnIOThread() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  if (stream_id_ && is_started_)
    Send(new AudioHostMsg_PlayStream(stream_id_));
  else
    play_on_start_ = true;
}

void AudioDevice::PauseOnIOThread(bool flush) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  if (stream_id_ && is_started_) {
    Send(new AudioHostMsg_PauseStream(stream_id_));
    if (flush)
      Send(new AudioHostMsg_FlushStream(stream_id_));
  } else {
    // Note that |flush| isn't relevant here since this is the case where
    // the stream is first starting.
    play_on_start_ = false;
  }
}

void AudioDevice::ShutDownOnIOThread(base::WaitableEvent* completion) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  is_started_ = false;

  // Make sure we don't call shutdown more than once.
  if (!stream_id_) {
    if (completion)
      completion->Signal();
    return;
  }

  filter_->RemoveDelegate(stream_id_);
  Send(new AudioHostMsg_CloseStream(stream_id_));
  stream_id_ = 0;

  if (completion)
    completion->Signal();
}

void AudioDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  if (stream_id_)
    Send(new AudioHostMsg_SetVolume(stream_id_, volume));
}

void AudioDevice::OnRequestPacket(AudioBuffersState buffers_state) {
  // This method does not apply to the low-latency system.
}

void AudioDevice::OnStateChanged(AudioStreamState state) {
  if (state == kAudioStreamError) {
    DLOG(WARNING) << "AudioDevice::OnStateChanged(kError)";
  }
}

void AudioDevice::OnCreated(
    base::SharedMemoryHandle handle, uint32 length) {
  // Not needed in this simple implementation.
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
  DCHECK(!audio_thread_.get());

  // Takes care of the case when Stop() is called before OnLowLatencyCreated().
  if (!stream_id_) {
    base::SharedMemory::CloseHandle(handle);
    // Close the socket handler.
    base::SyncSocket socket(socket_handle);
    return;
  }

  shared_memory_handle_ = handle;
  memory_length_ = length;

  DCHECK_GE(length, buffer_size_ * sizeof(int16) * channels_);

  audio_socket_ = new AudioSocket(socket_handle);
  {
    // Synchronize with ShutDownAudioThread().
    base::AutoLock auto_lock(lock_);

    DCHECK(!audio_thread_.get());
    audio_thread_.reset(
        new base::DelegateSimpleThread(this, "renderer_audio_thread"));
    audio_thread_->Start();
  }

  // We handle the case where Play() and/or Pause() may have been called
  // multiple times before OnLowLatencyCreated() gets called.
  is_started_ = true;
  if (play_on_start_)
    PlayOnIOThread();
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

  base::SharedMemory shared_memory(shared_memory_handle_, false);
  shared_memory.Map(media::TotalSharedMemorySizeInBytes(memory_length_));
  scoped_refptr<AudioSocket> audio_socket(audio_socket_);

  int pending_data;
  const int samples_per_ms = static_cast<int>(sample_rate_) / 1000;
  const int bytes_per_ms = channels_ * (bits_per_sample_ / 8) * samples_per_ms;

  while (sizeof(pending_data) ==
      audio_socket->socket()->Receive(&pending_data, sizeof(pending_data))) {
    if (pending_data == media::AudioOutputController::kPauseMark) {
      memset(shared_memory.memory(), 0, memory_length_);
      media::SetActualDataSizeInBytes(&shared_memory, memory_length_, 0);
      continue;
    } else if (pending_data < 0) {
      break;
    }

    // Convert the number of pending bytes in the render buffer
    // into milliseconds.
    audio_delay_milliseconds_ = pending_data / bytes_per_ms;
    size_t num_frames = FireRenderCallback(
        reinterpret_cast<int16*>(shared_memory.memory()));

    // Let the host know we are done.
    media::SetActualDataSizeInBytes(&shared_memory,
                                    memory_length_,
                                    num_frames * channels_ * sizeof(int16));
  }
  audio_socket->Close();
}

size_t AudioDevice::FireRenderCallback(int16* data) {
  TRACE_EVENT0("audio", "AudioDevice::FireRenderCallback");

  size_t num_frames = 0;
  if (callback_) {
    // Update the audio-delay measurement then ask client to render audio.
    num_frames = callback_->Render(audio_data_,
                                   buffer_size_,
                                   audio_delay_milliseconds_);

    // Interleave, scale, and clip to int16.
    // TODO(crogers): avoid converting to integer here, and pass the data
    // to the browser process as float, so we don't lose precision for
    // audio hardware which has better than 16bit precision.
    media::InterleaveFloatToInt16(audio_data_,
                                  data,
                                  buffer_size_);
  }
  return num_frames;
}

void AudioDevice::ShutDownAudioThread() {
  // Synchronize with OnLowLatencyCreated().
  base::AutoLock auto_lock(lock_);
  if (audio_thread_.get()) {
    // Close the socket to terminate the main thread function in the
    // audio thread.
    audio_socket_->Close();
    audio_thread_->Join();
    audio_thread_.reset(NULL);
  }
}
