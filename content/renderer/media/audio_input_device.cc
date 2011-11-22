// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_device.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_util.h"

AudioInputDevice::AudioInputDevice(size_t buffer_size,
                                   int channels,
                                   double sample_rate,
                                   CaptureCallback* callback,
                                   CaptureEventHandler* event_handler)
    : callback_(callback),
      event_handler_(event_handler),
      audio_delay_milliseconds_(0),
      volume_(1.0),
      stream_id_(0),
      session_id_(0),
      pending_device_ready_(false) {
  filter_ = RenderThreadImpl::current()->audio_input_message_filter();
  audio_data_.reserve(channels);
#if defined(OS_MACOSX)
  VLOG(1) << "Using AUDIO_PCM_LOW_LATENCY as input mode on Mac OS X.";
  audio_parameters_.format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
#elif defined(OS_WIN)
  VLOG(1) << "Using AUDIO_PCM_LOW_LATENCY as input mode on Windows.";
  audio_parameters_.format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
#else
  // TODO(henrika): add support for AUDIO_PCM_LOW_LATENCY on Linux as well.
  audio_parameters_.format = AudioParameters::AUDIO_PCM_LINEAR;
#endif
  audio_parameters_.channels = channels;
  audio_parameters_.sample_rate = static_cast<int>(sample_rate);
  audio_parameters_.bits_per_sample = 16;
  audio_parameters_.samples_per_packet = buffer_size;
  for (int i = 0; i < channels; ++i) {
    float* channel_data = new float[buffer_size];
    audio_data_.push_back(channel_data);
  }
}

AudioInputDevice::~AudioInputDevice() {
  // TODO(henrika): The current design requires that the user calls
  // Stop before deleting this class.
  CHECK_EQ(0, stream_id_);
  for (int i = 0; i < audio_parameters_.channels; ++i)
    delete [] audio_data_[i];
}

void AudioInputDevice::Start() {
  VLOG(1) << "Start()";
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDevice::InitializeOnIOThread, this));
}

void AudioInputDevice::SetDevice(int session_id) {
  VLOG(1) << "SetDevice (session_id=" << session_id << ")";
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDevice::SetSessionIdOnIOThread, this,
                 session_id));
}

bool AudioInputDevice::Stop() {
  VLOG(1) << "Stop()";
  // Max waiting time for Stop() to complete. If this time limit is passed,
  // we will stop waiting and return false. It ensures that Stop() can't block
  // the calling thread forever.
  const base::TimeDelta kMaxTimeOut = base::TimeDelta::FromMilliseconds(1000);

  base::WaitableEvent completion(false, false);

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDevice::ShutDownOnIOThread, this,
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

void AudioInputDevice::InitializeOnIOThread() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  // Make sure we don't call Start() more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return;

  stream_id_ = filter_->AddDelegate(this);
  // If |session_id_| is not specified, it will directly create the stream;
  // otherwise it will send a AudioInputHostMsg_StartDevice msg to the browser
  // and create the stream when getting a OnDeviceReady() callback.
  if (!session_id_) {
    Send(new AudioInputHostMsg_CreateStream(
        stream_id_, audio_parameters_, true,
        AudioManagerBase::kDefaultDeviceId));
  } else {
    Send(new AudioInputHostMsg_StartDevice(stream_id_, session_id_));
    pending_device_ready_ = true;
  }
}

void AudioInputDevice::SetSessionIdOnIOThread(int session_id) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  session_id_ = session_id;
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
  session_id_ = 0;
  pending_device_ready_ = false;

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
    // Close the socket handler.
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
      base::Bind(&AudioInputDevice::StartOnIOThread, this));
}

void AudioInputDevice::OnVolume(double volume) {
  NOTIMPLEMENTED();
}

void AudioInputDevice::OnStateChanged(AudioStreamState state) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  switch (state) {
    case kAudioStreamPaused:
      // Do nothing if the stream has been closed.
      if (!stream_id_)
        return;

      filter_->RemoveDelegate(stream_id_);

      // Joining the audio thread will be quite soon, since the stream has
      // been closed before.
      if (audio_thread_.get()) {
        socket_->Close();
        audio_thread_->Join();
        audio_thread_.reset(NULL);
      }

      if (event_handler_)
        event_handler_->OnDeviceStopped();

      stream_id_ = 0;
      pending_device_ready_ = false;
      break;
    case kAudioStreamPlaying:
      NOTIMPLEMENTED();
      break;
    case kAudioStreamError:
      DLOG(WARNING) << "AudioInputDevice::OnStateChanged(kError)";
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AudioInputDevice::OnDeviceReady(const std::string& device_id) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  VLOG(1) << "OnDeviceReady (device_id=" << device_id << ")";

  // Takes care of the case when Stop() is called before OnDeviceReady().
  if (!pending_device_ready_)
    return;

  // If AudioInputDeviceManager returns an empty string, it means no device
  // is ready for start.
  if (device_id.empty()) {
    filter_->RemoveDelegate(stream_id_);
    stream_id_ = 0;
  } else {
    Send(new AudioInputHostMsg_CreateStream(
        stream_id_, audio_parameters_, true, device_id));
  }

  pending_device_ready_ = false;
  // Notify the client that the device has been started.
  if (event_handler_)
    event_handler_->OnDeviceStarted(device_id);
}

void AudioInputDevice::Send(IPC::Message* message) {
  filter_->Send(message);
}

// Our audio thread runs here. We receive captured audio samples on
// this thread.
void AudioInputDevice::Run() {
  audio_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  int pending_data;
  const int samples_per_ms =
      static_cast<int>(audio_parameters_.sample_rate) / 1000;
  const int bytes_per_ms = audio_parameters_.channels *
      (audio_parameters_.bits_per_sample / 8) * samples_per_ms;

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

  const size_t number_of_frames = audio_parameters_.samples_per_packet;

  // Read 16-bit samples from shared memory (browser writes to it).
  int16* input_audio = static_cast<int16*>(shared_memory_data());
  const int bytes_per_sample = sizeof(input_audio[0]);

  // Deinterleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0.
  for (int channel_index = 0; channel_index < audio_parameters_.channels;
       ++channel_index) {
    media::DeinterleaveAudioChannel(input_audio,
                                    audio_data_[channel_index],
                                    audio_parameters_.channels,
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
