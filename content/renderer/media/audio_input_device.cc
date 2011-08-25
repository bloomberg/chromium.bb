// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_device.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "media/audio/audio_util.h"

// Max waiting time for Stop() to complete. If this time limit is passed,
// we will stop waiting and return false. It ensures that Stop() can't block
// the calling thread forever.
static const base::TimeDelta kMaxTimeOut =
    base::TimeDelta::FromMilliseconds(1000);

AudioInputDevice::AudioInputDevice(size_t buffer_size,
                                   int channels,
                                   double sample_rate,
                                   CaptureCallback* callback)
    : buffer_size_(buffer_size),
      channels_(channels),
      bits_per_sample_(16),
      sample_rate_(sample_rate),
      callback_(callback),
      audio_delay_milliseconds_(0),
      volume_(1.0),
      stream_id_(0) {
  filter_ = RenderThread::current()->audio_input_message_filter();
  audio_data_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    float* channel_data = new float[buffer_size];
    audio_data_.push_back(channel_data);
  }
}

AudioInputDevice::~AudioInputDevice() {
  // TODO(henrika): The current design requires that the user calls
  // Stop before deleting this class.
  CHECK_EQ(0, stream_id_);
  for (int i = 0; i < channels_; ++i)
    delete [] audio_data_[i];
}

void AudioInputDevice::Start() {
  VLOG(1) << "Start()";
  AudioParameters params;
  // TODO(henrika): add support for low-latency mode?
  params.format = AudioParameters::AUDIO_PCM_LINEAR;
  params.channels = channels_;
  params.sample_rate = static_cast<int>(sample_rate_);
  params.bits_per_sample = bits_per_sample_;
  params.samples_per_packet = buffer_size_;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioInputDevice::InitializeOnIOThread, params));
}

bool AudioInputDevice::Stop() {
  VLOG(1) << "Stop()";
  base::WaitableEvent completion(false, false);

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioInputDevice::ShutDownOnIOThread,
                        &completion));

  // We wait here for the IO task to be completed to remove race conflicts
  // with OnLowLatencyCreated() and to ensure that Stop() acts as a synchronous
  // function call.
  if (completion.TimedWait(kMaxTimeOut)) {
    if (audio_thread_.get()) {
      // Terminate the main thread function in the audio thread.
      socket_->Close();
      // Wait for the audio thread to exit.
      audio_thread_->Join();
      // Ensures that we can call Stop() multiple times.
      audio_thread_.reset(NULL);
    }
  } else {
    LOG(ERROR) << "Failed to shut down audio input on IO thread";
    return false;
  }

  return true;
}

bool AudioInputDevice::SetVolume(double volume) {
  NOTIMPLEMENTED();
  return false;
}

bool AudioInputDevice::GetVolume(double* volume) {
  NOTIMPLEMENTED();
  return false;
}

void AudioInputDevice::InitializeOnIOThread(const AudioParameters& params) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  // Make sure we don't call Start() more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return;

  stream_id_ = filter_->AddDelegate(this);
  Send(new AudioInputHostMsg_CreateStream(stream_id_, params, true));
}

void AudioInputDevice::StartOnIOThread() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  if (stream_id_)
    Send(new AudioInputHostMsg_RecordStream(stream_id_));
}

void AudioInputDevice::ShutDownOnIOThread(base::WaitableEvent* completion) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  // Make sure we don't call shutdown more than once.
  if (!stream_id_) {
    completion->Signal();
    return;
  }

  filter_->RemoveDelegate(stream_id_);
  Send(new AudioInputHostMsg_CloseStream(stream_id_));
  stream_id_ = 0;

  completion->Signal();
}

void AudioInputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  if (stream_id_)
    Send(new AudioInputHostMsg_SetVolume(stream_id_, volume));
}

void AudioInputDevice::OnLowLatencyCreated(
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

  VLOG(1) << "OnLowLatencyCreated (stream_id=" << stream_id_ << ")";
  // Takes care of the case when Stop() is called before OnLowLatencyCreated().
  if (!stream_id_) {
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);

  socket_.reset(new base::SyncSocket(socket_handle));

  audio_thread_.reset(
      new base::DelegateSimpleThread(this, "RendererAudioInputThread"));
  audio_thread_->Start();

  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioInputDevice::StartOnIOThread));
}

void AudioInputDevice::OnVolume(double volume) {
  NOTIMPLEMENTED();
}

void AudioInputDevice::Send(IPC::Message* message) {
  filter_->Send(message);
}

// Our audio thread runs here. We receive captured audio samples on
// this thread.
void AudioInputDevice::Run() {
  audio_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  int pending_data;
  const int samples_per_ms = static_cast<int>(sample_rate_) / 1000;
  const int bytes_per_ms = channels_ * (bits_per_sample_ / 8) * samples_per_ms;

  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
                                                  pending_data >= 0) {
    // TODO(henrika): investigate the provided |pending_data| value
    // and ensure that it is actually an accurate delay estimation.

    // Convert the number of pending bytes in the capture buffer
    // into milliseconds.
    audio_delay_milliseconds_ = pending_data / bytes_per_ms;

    FireCaptureCallback();
  }
}

void AudioInputDevice::FireCaptureCallback() {
  if (!callback_)
      return;

  const size_t number_of_frames = buffer_size_;

  // Read 16-bit samples from shared memory (browser writes to it).
  int16* input_audio = static_cast<int16*>(shared_memory_data());
  const int bytes_per_sample = sizeof(input_audio[0]);

  // Deinterleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0.
  for (int channel_index = 0; channel_index < channels_; ++channel_index) {
    media::DeinterleaveAudioChannel(input_audio,
                                    audio_data_[channel_index],
                                    channels_,
                                    channel_index,
                                    bytes_per_sample,
                                    number_of_frames);
  }

  // Deliver captured data to the client in floating point format
  // and update the audio-delay measurement.
  callback_->Capture(audio_data_,
                     number_of_frames,
                     audio_delay_milliseconds_);
}
