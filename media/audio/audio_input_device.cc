// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_device.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

namespace media {

namespace {

// The number of shared memory buffer segments indicated to browser process
// in order to avoid data overwriting. This number can be any positive number,
// dependent how fast the renderer process can pick up captured data from
// shared memory.
const int kRequestedSharedMemoryCount = 10;

// The number of seconds with missing callbacks before we report a capture
// error. The value is based on that the Mac audio implementation can defer
// start for 5 seconds when resuming after standby, and has a startup success
// check 5 seconds after actually starting, where stats is logged. We must allow
// enough time for this. See AUAudioInputStream::CheckInputStartupSuccess().
const int kMissingCallbacksTimeBeforeErrorSeconds = 12;

// The interval for checking missing callbacks.
const int kCheckMissingCallbacksIntervalSeconds = 5;

// How often AudioInputDevice::AudioThreadCallback informs that it has gotten
// data from the source.
const int kGotDataCallbackIntervalSeconds = 1;

}  // namespace

// Takes care of invoking the capture callback on the audio thread.
// An instance of this class is created for each capture stream in
// OnLowLatencyCreated().
class AudioInputDevice::AudioThreadCallback
    : public AudioDeviceThread::Callback {
 public:
  AudioThreadCallback(const AudioParameters& audio_parameters,
                      base::SharedMemoryHandle memory,
                      int memory_length,
                      int total_segments,
                      CaptureCallback* capture_callback,
                      base::RepeatingClosure got_data_callback);
  ~AudioThreadCallback() override;

  void MapSharedMemory() override;

  // Called whenever we receive notifications about pending data.
  void Process(uint32_t pending_data) override;

 private:
  const double bytes_per_ms_;
  int current_segment_id_;
  uint32_t last_buffer_id_;
  std::vector<std::unique_ptr<media::AudioBus>> audio_buses_;
  CaptureCallback* capture_callback_;

  // Used for informing AudioInputDevice that we have gotten data, i.e. the
  // stream is alive. |got_data_callback_| is run every
  // |got_data_callback_interval_in_frames_| frames, calculated from
  // kGotDataCallbackIntervalSeconds.
  const int got_data_callback_interval_in_frames_;
  int frames_since_last_got_data_callback_;
  base::RepeatingClosure got_data_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioThreadCallback);
};

AudioInputDevice::AudioInputDevice(
    std::unique_ptr<AudioInputIPC> ipc,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : ScopedTaskRunnerObserver(io_task_runner),
      callback_(NULL),
      ipc_(std::move(ipc)),
      state_(IDLE),
      session_id_(0),
      agc_is_enabled_(false),
      stopping_hack_(false),
      missing_callbacks_detected_(false) {
  CHECK(ipc_);

  // The correctness of the code depends on the relative values assigned in the
  // State enum.
  static_assert(IPC_CLOSED < IDLE, "invalid enum value assignment 0");
  static_assert(IDLE < CREATING_STREAM, "invalid enum value assignment 1");
  static_assert(CREATING_STREAM < RECORDING, "invalid enum value assignment 2");
}

void AudioInputDevice::Initialize(const AudioParameters& params,
                                  CaptureCallback* callback,
                                  int session_id) {
  DCHECK(params.IsValid());
  DCHECK(!callback_);
  DCHECK_EQ(0, session_id_);
  audio_parameters_ = params;
  callback_ = callback;
  session_id_ = session_id;
}

void AudioInputDevice::Start() {
  DCHECK(callback_) << "Initialize hasn't been called";
  DVLOG(1) << "Start()";
  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::StartUpOnIOThread, this));
}

void AudioInputDevice::Stop() {
  DVLOG(1) << "Stop()";

  {
    base::AutoLock auto_lock(audio_thread_lock_);
    audio_thread_.reset();
    stopping_hack_ = true;
  }

  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::ShutDownOnIOThread, this));
}

void AudioInputDevice::SetVolume(double volume) {
  if (volume < 0 || volume > 1.0) {
    DLOG(ERROR) << "Invalid volume value specified";
    return;
  }

  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::SetVolumeOnIOThread, this, volume));
}

void AudioInputDevice::SetAutomaticGainControl(bool enabled) {
  DVLOG(1) << "SetAutomaticGainControl(enabled=" << enabled << ")";
  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::SetAutomaticGainControlOnIOThread,
          this, enabled));
}

void AudioInputDevice::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    int length,
    int total_segments) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK(base::SharedMemory::IsHandleValid(handle));
#if defined(OS_WIN)
  DCHECK(socket_handle);
#else
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK_GT(length, 0);

  if (state_ != CREATING_STREAM)
    return;

  base::AutoLock auto_lock(audio_thread_lock_);
  // TODO(miu): See TODO in OnStreamCreated method for AudioOutputDevice.
  // Interface changes need to be made; likely, after AudioInputDevice is merged
  // into AudioOutputDevice (http://crbug.com/179597).
  if (stopping_hack_)
    return;

  DCHECK(!audio_callback_);
  DCHECK(!audio_thread_);
  audio_callback_.reset(new AudioInputDevice::AudioThreadCallback(
      audio_parameters_, handle, length, total_segments, callback_,
      base::BindRepeating(&AudioInputDevice::SetLastCallbackTimeToNow, this)));
  audio_thread_.reset(new AudioDeviceThread(audio_callback_.get(),
                                            socket_handle, "AudioInputDevice"));

  state_ = RECORDING;
  ipc_->RecordStream();

  // Start detecting missing callbacks.
  SetLastCallbackTimeToNowOnIOThread();
  DCHECK(!check_alive_timer_);
  check_alive_timer_ = base::MakeUnique<base::RepeatingTimer>();
  check_alive_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kCheckMissingCallbacksIntervalSeconds), this,
      &AudioInputDevice::CheckIfInputStreamIsAlive);
  DCHECK(check_alive_timer_->IsRunning());
}

void AudioInputDevice::OnError() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
    return;

  DLOG(WARNING) << "AudioInputDevice::OnStateChanged(ERROR)";
  if (state_ == CREATING_STREAM) {
    // At this point, we haven't attempted to start the audio thread.
    // Accessing the hardware might have failed or we may have reached
    // the limit of the number of allowed concurrent streams.
    // We must report the error to the |callback_| so that a potential
    // audio source object will enter the correct state (e.g. 'ended' for
    // a local audio source).
    callback_->OnCaptureError(
        "Maximum allowed input device limit reached or OS failure.");
  } else {
    // Don't dereference the callback object if the audio thread
    // is stopped or stopping.  That could mean that the callback
    // object has been deleted.
    // TODO(tommi): Add an explicit contract for clearing the callback
    // object.  Possibly require calling Initialize again or provide
    // a callback object via Start() and clear it in Stop().
    base::AutoLock auto_lock_(audio_thread_lock_);
    if (audio_thread_)
      callback_->OnCaptureError("IPC delegate state error.");
  }
}

void AudioInputDevice::OnMuted(bool is_muted) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
    return;
  callback_->OnCaptureMuted(is_muted);
}

void AudioInputDevice::OnIPCClosed() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  state_ = IPC_CLOSED;
  ipc_.reset();
}

AudioInputDevice::~AudioInputDevice() {
#if DCHECK_IS_ON()
  // Make sure we've stopped the stream properly before destructing |this|.
  DCHECK(audio_thread_lock_.Try());
  DCHECK_LE(state_, IDLE);
  DCHECK(!audio_thread_);
  DCHECK(!audio_callback_);
  DCHECK(!stopping_hack_);
  audio_thread_lock_.Release();
#endif  // DCHECK_IS_ON()
}

void AudioInputDevice::StartUpOnIOThread() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  // Make sure we don't call Start() more than once.
  if (state_ != IDLE)
    return;

  if (session_id_ <= 0) {
    DLOG(WARNING) << "Invalid session id for the input stream " << session_id_;
    return;
  }

  state_ = CREATING_STREAM;
  ipc_->CreateStream(this, session_id_, audio_parameters_,
                     agc_is_enabled_, kRequestedSharedMemoryCount);
}

void AudioInputDevice::ShutDownOnIOThread() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (check_alive_timer_) {
    check_alive_timer_->Stop();
    check_alive_timer_.reset();
  }

  UMA_HISTOGRAM_BOOLEAN("Media.Audio.Capture.DetectedMissingCallbacks",
                        missing_callbacks_detected_);
  missing_callbacks_detected_ = false;

  // Close the stream, if we haven't already.
  if (state_ >= CREATING_STREAM) {
    ipc_->CloseStream();
    state_ = IDLE;
    agc_is_enabled_ = false;
  }

  // We can run into an issue where ShutDownOnIOThread is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // Stop(). In most cases, the thread will already be stopped.
  //
  // Another situation is when the IO thread goes away before Stop() is called
  // in which case, we cannot use the message loop to close the thread handle
  // and can't not rely on the main thread existing either.
  base::AutoLock auto_lock_(audio_thread_lock_);
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  audio_thread_.reset();
  audio_callback_.reset();
  stopping_hack_ = false;
}

void AudioInputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  if (state_ >= CREATING_STREAM)
    ipc_->SetVolume(volume);
}

void AudioInputDevice::SetAutomaticGainControlOnIOThread(bool enabled) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (state_ >= CREATING_STREAM) {
    DLOG(WARNING) << "The AGC state can not be modified after starting.";
    return;
  }

  // We simply store the new AGC setting here. This value will be used when
  // a new stream is initialized and by GetAutomaticGainControl().
  agc_is_enabled_ = enabled;
}

void AudioInputDevice::WillDestroyCurrentMessageLoop() {
  LOG(ERROR) << "IO loop going away before the input device has been stopped";
  ShutDownOnIOThread();
}

void AudioInputDevice::CheckIfInputStreamIsAlive() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  if (base::TimeTicks::Now() - last_callback_time_ >
      base::TimeDelta::FromSeconds(kMissingCallbacksTimeBeforeErrorSeconds)) {
    callback_->OnCaptureError("No audio received from audio capture device.");
    missing_callbacks_detected_ = true;
  }
}

void AudioInputDevice::SetLastCallbackTimeToNow() {
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDevice::SetLastCallbackTimeToNowOnIOThread, this));
}

void AudioInputDevice::SetLastCallbackTimeToNowOnIOThread() {
  last_callback_time_ = base::TimeTicks::Now();
}

// AudioInputDevice::AudioThreadCallback
AudioInputDevice::AudioThreadCallback::AudioThreadCallback(
    const AudioParameters& audio_parameters,
    base::SharedMemoryHandle memory,
    int memory_length,
    int total_segments,
    CaptureCallback* capture_callback,
    base::RepeatingClosure got_data_callback_)
    : AudioDeviceThread::Callback(audio_parameters,
                                  memory,
                                  memory_length,
                                  total_segments),
      bytes_per_ms_(static_cast<double>(audio_parameters.GetBytesPerSecond()) /
                    base::Time::kMillisecondsPerSecond),
      current_segment_id_(0),
      last_buffer_id_(UINT32_MAX),
      capture_callback_(capture_callback),
      got_data_callback_interval_in_frames_(kGotDataCallbackIntervalSeconds *
                                            audio_parameters.sample_rate()),
      frames_since_last_got_data_callback_(0),
      got_data_callback_(std::move(got_data_callback_)) {}

AudioInputDevice::AudioThreadCallback::~AudioThreadCallback() {
}

void AudioInputDevice::AudioThreadCallback::MapSharedMemory() {
  shared_memory_.Map(memory_length_);

  // Create vector of audio buses by wrapping existing blocks of memory.
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_.memory());
  for (int i = 0; i < total_segments_; ++i) {
    media::AudioInputBuffer* buffer =
        reinterpret_cast<media::AudioInputBuffer*>(ptr);
    audio_buses_.push_back(
        media::AudioBus::WrapMemory(audio_parameters_, buffer->audio));
    ptr += segment_length_;
  }

  // Indicate that browser side capture initialization has succeeded and IPC
  // channel initialized. This effectively completes the
  // AudioCapturerSource::Start()' phase as far as the caller of that function
  // is concerned.
  capture_callback_->OnCaptureStarted();
}

void AudioInputDevice::AudioThreadCallback::Process(uint32_t pending_data) {
  // The shared memory represents parameters, size of the data buffer and the
  // actual data buffer containing audio data. Map the memory into this
  // structure and parse out parameters and the data area.
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_.memory());
  ptr += current_segment_id_ * segment_length_;
  AudioInputBuffer* buffer = reinterpret_cast<AudioInputBuffer*>(ptr);

  // Usually this will be equal but in the case of low sample rate (e.g. 8kHz,
  // the buffer may be bigger (on mac at least)).
  DCHECK_GE(buffer->params.size,
            segment_length_ - sizeof(AudioInputBufferParameters));

  // Verify correct sequence.
  if (buffer->params.id != last_buffer_id_ + 1) {
    std::string message = base::StringPrintf(
        "Incorrect buffer sequence. Expected = %u. Actual = %u.",
        last_buffer_id_ + 1, buffer->params.id);
    LOG(ERROR) << message;
    capture_callback_->OnCaptureError(message);
  }
  if (current_segment_id_ != static_cast<int>(pending_data)) {
    std::string message = base::StringPrintf(
        "Segment id not matching. Remote = %u. Local = %d.",
        pending_data, current_segment_id_);
    LOG(ERROR) << message;
    capture_callback_->OnCaptureError(message);
  }
  last_buffer_id_ = buffer->params.id;

  // Use pre-allocated audio bus wrapping existing block of shared memory.
  media::AudioBus* audio_bus = audio_buses_[current_segment_id_].get();

  // Regularly inform that we have gotten data.
  frames_since_last_got_data_callback_ += audio_bus->frames();
  if (frames_since_last_got_data_callback_ >=
      got_data_callback_interval_in_frames_) {
    got_data_callback_.Run();
    frames_since_last_got_data_callback_ = 0;
  }

  // Deliver captured data to the client in floating point format and update
  // the audio delay measurement.
  capture_callback_->Capture(
      audio_bus,
      buffer->params.hardware_delay_bytes / bytes_per_ms_,  // Delay in ms
      buffer->params.volume, buffer->params.key_pressed);

  if (++current_segment_id_ >= total_segments_)
    current_segment_id_ = 0;
}

}  // namespace media
