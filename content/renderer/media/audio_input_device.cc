// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_device.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_util.h"

// Takes care of invoking the capture callback on the audio thread.
// An instance of this class is created for each capture stream in
// OnLowLatencyCreated().
class AudioInputDevice::AudioThreadCallback
    : public AudioDeviceThread::Callback {
 public:
  AudioThreadCallback(const media::AudioParameters& audio_parameters,
                      base::SharedMemoryHandle memory,
                      int memory_length,
                      CaptureCallback* capture_callback);
  virtual ~AudioThreadCallback();

  virtual void MapSharedMemory() OVERRIDE;

  // Called whenever we receive notifications about pending data.
  virtual void Process(int pending_data) OVERRIDE;

 private:
  CaptureCallback* capture_callback_;
  DISALLOW_COPY_AND_ASSIGN(AudioThreadCallback);
};

AudioInputDevice::AudioInputDevice(const media::AudioParameters& params,
                                   CaptureCallback* callback,
                                   CaptureEventHandler* event_handler)
    : ScopedLoopObserver(ChildProcess::current()->io_message_loop()),
      audio_parameters_(params),
      callback_(callback),
      event_handler_(event_handler),
      volume_(1.0),
      stream_id_(0),
      session_id_(0),
      pending_device_ready_(false),
      agc_is_enabled_(false) {
  filter_ = RenderThreadImpl::current()->audio_input_message_filter();
}

void AudioInputDevice::SetDevice(int session_id) {
  DVLOG(1) << "SetDevice (session_id=" << session_id << ")";
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::SetSessionIdOnIOThread, this, session_id));
}

void AudioInputDevice::Start() {
  DVLOG(1) << "Start()";
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::InitializeOnIOThread, this));
}

void AudioInputDevice::Stop() {
  DVLOG(1) << "Stop()";

  {
    base::AutoLock auto_lock(audio_thread_lock_);
    audio_thread_.Stop(MessageLoop::current());
  }

  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::ShutDownOnIOThread, this));
}

bool AudioInputDevice::SetVolume(double volume) {
  if (volume < 0 || volume > 1.0)
    return false;

  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::SetVolumeOnIOThread, this, volume));

  return true;
}

bool AudioInputDevice::GetVolume(double* volume) {
  NOTREACHED();
  return false;
}

void AudioInputDevice::SetAutomaticGainControl(bool enabled) {
  DVLOG(1) << "SetAutomaticGainControl(enabled=" << enabled << ")";
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::SetAutomaticGainControlOnIOThread,
          this, enabled));
}

void AudioInputDevice::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    uint32 length) {
  DCHECK(message_loop()->BelongsToCurrentThread());
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK(length);
  DVLOG(1) << "OnStreamCreated (stream_id=" << stream_id_ << ")";

  base::AutoLock auto_lock(audio_thread_lock_);

  // Takes care of the case when Stop() is called before OnStreamCreated().
  if (!stream_id_) {
    base::SharedMemory::CloseHandle(handle);
    // Close the socket handler.
    base::SyncSocket socket(socket_handle);
    return;
  }

  DCHECK(audio_thread_.IsStopped());
  audio_callback_.reset(
      new AudioInputDevice::AudioThreadCallback(audio_parameters_, handle,
                                                length, callback_));
  audio_thread_.Start(audio_callback_.get(), socket_handle, "AudioInputDevice");

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::StartOnIOThread, this));
}

void AudioInputDevice::OnVolume(double volume) {
  NOTIMPLEMENTED();
}

void AudioInputDevice::OnStateChanged(AudioStreamState state) {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (!stream_id_)
    return;

  switch (state) {
    // TODO(xians): This should really be kAudioStreamStopped since the stream
    // has been closed at this point.
    case kAudioStreamPaused:
      // TODO(xians): Should we just call ShutDownOnIOThread here instead?
      filter_->RemoveDelegate(stream_id_);

      audio_thread_.Stop(MessageLoop::current());
      audio_callback_.reset();

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
      // Don't dereference the callback object if the audio thread
      // is stopped or stopping.  That could mean that the callback
      // object has been deleted.
      // TODO(tommi): Add an explicit contract for clearing the callback
      // object.  Possibly require calling Initialize again or provide
      // a callback object via Start() and clear it in Stop().
      if (!audio_thread_.IsStopped())
        callback_->OnCaptureError();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AudioInputDevice::OnDeviceReady(const std::string& device_id) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  DVLOG(1) << "OnDeviceReady (device_id=" << device_id << ")";

  // Takes care of the case when Stop() is called before OnDeviceReady().
  if (!pending_device_ready_)
    return;

  // If AudioInputDeviceManager returns an empty string, it means no device
  // is ready for start.
  if (device_id.empty()) {
    filter_->RemoveDelegate(stream_id_);
    stream_id_ = 0;
  } else {
    Send(new AudioInputHostMsg_CreateStream(stream_id_, audio_parameters_,
                                            device_id, agc_is_enabled_));
  }

  pending_device_ready_ = false;
  // Notify the client that the device has been started.
  if (event_handler_)
    event_handler_->OnDeviceStarted(device_id);
}

AudioInputDevice::~AudioInputDevice() {
  // TODO(henrika): The current design requires that the user calls
  // Stop before deleting this class.
  CHECK_EQ(0, stream_id_);
}

void AudioInputDevice::InitializeOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());
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
        stream_id_, audio_parameters_,
        media::AudioManagerBase::kDefaultDeviceId,
        agc_is_enabled_));
  } else {
    Send(new AudioInputHostMsg_StartDevice(stream_id_, session_id_));
    pending_device_ready_ = true;
  }
}

void AudioInputDevice::SetSessionIdOnIOThread(int session_id) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  session_id_ = session_id;
}

void AudioInputDevice::StartOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (stream_id_)
    Send(new AudioInputHostMsg_RecordStream(stream_id_));
}

void AudioInputDevice::ShutDownOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  // NOTE: |completion| may be NULL.
  // Make sure we don't call shutdown more than once.
  if (stream_id_) {
    filter_->RemoveDelegate(stream_id_);
    Send(new AudioInputHostMsg_CloseStream(stream_id_));

    stream_id_ = 0;
    session_id_ = 0;
    pending_device_ready_ = false;
    agc_is_enabled_ = false;
  }

  // We can run into an issue where ShutDownOnIOThread is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // Stop(). In most cases, the thread will already be stopped.
  // Another situation is when the IO thread goes away before Stop() is called
  // in which case, we cannot use the message loop to close the thread handle
  // and can't not rely on the main thread existing either.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  audio_thread_.Stop(NULL);
  audio_callback_.reset();
}

void AudioInputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (stream_id_)
    Send(new AudioInputHostMsg_SetVolume(stream_id_, volume));
}

void AudioInputDevice::SetAutomaticGainControlOnIOThread(bool enabled) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  DCHECK_EQ(0, stream_id_) <<
      "The AGC state can not be modified while capturing is active.";
  if (stream_id_)
    return;

  // We simply store the new AGC setting here. This value will be used when
  // a new stream is initialized and by GetAutomaticGainControl().
  agc_is_enabled_ = enabled;
}

void AudioInputDevice::Send(IPC::Message* message) {
  filter_->Send(message);
}

void AudioInputDevice::WillDestroyCurrentMessageLoop() {
  LOG(ERROR) << "IO loop going away before the input device has been stopped";
  ShutDownOnIOThread();
}

// AudioInputDevice::AudioThreadCallback
AudioInputDevice::AudioThreadCallback::AudioThreadCallback(
    const media::AudioParameters& audio_parameters,
    base::SharedMemoryHandle memory,
    int memory_length,
    CaptureCallback* capture_callback)
    : AudioDeviceThread::Callback(audio_parameters, memory, memory_length),
      capture_callback_(capture_callback) {
}

AudioInputDevice::AudioThreadCallback::~AudioThreadCallback() {
}

void AudioInputDevice::AudioThreadCallback::MapSharedMemory() {
  shared_memory_.Map(memory_length_);
}

void AudioInputDevice::AudioThreadCallback::Process(int pending_data) {
  // The shared memory represents parameters, size of the data buffer and the
  // actual data buffer containing audio data. Map the memory into this
  // structure and parse out parameters and the data area.
  media::AudioInputBuffer* buffer =
      reinterpret_cast<media::AudioInputBuffer*>(shared_memory_.memory());
  uint32 size = buffer->params.size;
  DCHECK_EQ(size, memory_length_ - sizeof(media::AudioInputBufferParameters));
  double volume = buffer->params.volume;

  int audio_delay_milliseconds = pending_data / bytes_per_ms_;
  int16* memory = reinterpret_cast<int16*>(&buffer->audio[0]);
  const size_t number_of_frames = audio_parameters_.frames_per_buffer();
  const int bytes_per_sample = sizeof(memory[0]);

  // Deinterleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0.
  for (int channel_index = 0; channel_index < audio_parameters_.channels();
       ++channel_index) {
    media::DeinterleaveAudioChannel(memory,
                                    audio_data_[channel_index],
                                    audio_parameters_.channels(),
                                    channel_index,
                                    bytes_per_sample,
                                    number_of_frames);
  }

  // Deliver captured data to the client in floating point format
  // and update the audio-delay measurement.
  capture_callback_->Capture(audio_data_, number_of_frames,
                             audio_delay_milliseconds, volume);
}
