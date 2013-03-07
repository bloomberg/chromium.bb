// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_output_resampler.h"
#include "media/audio/audio_util.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/base/media_switches.h"

namespace media {

static const int kStreamCloseDelaySeconds = 5;

// Default maximum number of output streams that can be open simultaneously
// for all platforms.
static const int kDefaultMaxOutputStreams = 16;

// Default maximum number of input streams that can be open simultaneously
// for all platforms.
static const int kDefaultMaxInputStreams = 16;

static const int kMaxInputChannels = 2;

const char AudioManagerBase::kDefaultDeviceName[] = "Default";
const char AudioManagerBase::kDefaultDeviceId[] = "default";

AudioManagerBase::AudioManagerBase()
    : num_active_input_streams_(0),
      max_num_output_streams_(kDefaultMaxOutputStreams),
      max_num_input_streams_(kDefaultMaxInputStreams),
      num_output_streams_(0),
      num_input_streams_(0),
      output_listeners_(
          ObserverList<AudioDeviceListener>::NOTIFY_EXISTING_ONLY),
      audio_thread_(new base::Thread("AudioThread")) {
#if defined(OS_WIN)
  audio_thread_->init_com_with_mta(true);
#endif
#if defined(OS_MACOSX)
  // On Mac, use a UI loop to get native message pump so that CoreAudio property
  // listener callbacks fire.
  CHECK(audio_thread_->StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_UI, 0)));
#else
  CHECK(audio_thread_->Start());
#endif
  message_loop_ = audio_thread_->message_loop_proxy();
}

AudioManagerBase::~AudioManagerBase() {
  // The platform specific AudioManager implementation must have already
  // stopped the audio thread. Otherwise, we may destroy audio streams before
  // stopping the thread, resulting an unexpected behavior.
  // This way we make sure activities of the audio streams are all stopped
  // before we destroy them.
  CHECK(!audio_thread_.get());
  // All the output streams should have been deleted.
  DCHECK_EQ(0, num_output_streams_);
  // All the input streams should have been deleted.
  DCHECK_EQ(0, num_input_streams_);
}

string16 AudioManagerBase::GetAudioInputDeviceModel() {
  return string16();
}

scoped_refptr<base::MessageLoopProxy> AudioManagerBase::GetMessageLoop() {
  return message_loop_;
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStream(
    const AudioParameters& params) {
  // TODO(miu): Fix ~50 call points across several unit test modules to call
  // this method on the audio thread, then uncomment the following:
  // DCHECK(message_loop_->BelongsToCurrentThread());

  if (!params.IsValid()) {
    DLOG(ERROR) << "Audio parameters are invalid";
    return NULL;
  }

  // Limit the number of audio streams opened. This is to prevent using
  // excessive resources for a large number of audio streams. More
  // importantly it prevents instability on certain systems.
  // See bug: http://crbug.com/30242.
  if (num_output_streams_ >= max_num_output_streams_) {
    DLOG(ERROR) << "Number of opened output audio streams "
                << num_output_streams_
                << " exceed the max allowed number "
                << max_num_output_streams_;
    return NULL;
  }

  AudioOutputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      stream = MakeLinearOutputStream(params);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyOutputStream(params);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioOutputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = NULL;
      break;
  }

  if (stream)
    ++num_output_streams_;

  return stream;
}

AudioInputStream* AudioManagerBase::MakeAudioInputStream(
    const AudioParameters& params, const std::string& device_id) {
  // TODO(miu): Fix ~20 call points across several unit test modules to call
  // this method on the audio thread, then uncomment the following:
  // DCHECK(message_loop_->BelongsToCurrentThread());

  if (!params.IsValid() || (params.channels() > kMaxInputChannels) ||
      device_id.empty()) {
    DLOG(ERROR) << "Audio parameters are invalid for device " << device_id;
    return NULL;
  }

  if (num_input_streams_ >= max_num_input_streams_) {
    DLOG(ERROR) << "Number of opened input audio streams "
                << num_input_streams_
                << " exceed the max allowed number " << max_num_input_streams_;
    return NULL;
  }

  AudioInputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      stream = MakeLinearInputStream(params, device_id);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyInputStream(params, device_id);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioInputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = NULL;
      break;
  }

  if (stream)
    ++num_input_streams_;

  return stream;
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStreamProxy(
    const AudioParameters& params) {
#if defined(OS_IOS)
  // IOS implements audio input only.
  NOTIMPLEMENTED();
  return NULL;
#else
  DCHECK(message_loop_->BelongsToCurrentThread());

  bool use_audio_output_resampler =
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutputResampler) &&
      params.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY;

  // If we're not using AudioOutputResampler our output parameters are the same
  // as our input parameters.
  AudioParameters output_params = params;
  if (use_audio_output_resampler) {
    output_params = GetPreferredLowLatencyOutputStreamParameters(params);

    // Ensure we only pass on valid output parameters.
    if (!output_params.IsValid()) {
      // We've received invalid audio output parameters, so switch to a mock
      // output device based on the input parameters.  This may happen if the OS
      // provided us junk values for the hardware configuration.
      LOG(ERROR) << "Invalid audio output parameters received; using fake "
                 << "audio path. Channels: " << output_params.channels() << ", "
                 << "Sample Rate: " << output_params.sample_rate() << ", "
                 << "Bits Per Sample: " << output_params.bits_per_sample()
                 << ", Frames Per Buffer: "
                 << output_params.frames_per_buffer();

      // Tell the AudioManager to create a fake output device.
      output_params = AudioParameters(
          AudioParameters::AUDIO_FAKE, params.channel_layout(),
          params.sample_rate(), params.bits_per_sample(),
          params.frames_per_buffer());
    }
  }

  std::pair<AudioParameters, AudioParameters> dispatcher_key =
      std::make_pair(params, output_params);
  AudioOutputDispatchersMap::iterator it =
      output_dispatchers_.find(dispatcher_key);
  if (it != output_dispatchers_.end())
    return new AudioOutputProxy(it->second);

  base::TimeDelta close_delay =
      base::TimeDelta::FromSeconds(kStreamCloseDelaySeconds);

  if (use_audio_output_resampler &&
      output_params.format() != AudioParameters::AUDIO_FAKE) {
    scoped_refptr<AudioOutputDispatcher> dispatcher =
        new AudioOutputResampler(this, params, output_params, close_delay);
    output_dispatchers_[dispatcher_key] = dispatcher;
    return new AudioOutputProxy(dispatcher);
  }

  scoped_refptr<AudioOutputDispatcher> dispatcher =
      new AudioOutputDispatcherImpl(this, output_params, close_delay);
  output_dispatchers_[dispatcher_key] = dispatcher;
  return new AudioOutputProxy(dispatcher);
#endif  // defined(OS_IOS)
}

void AudioManagerBase::ShowAudioInputSettings() {
}

void AudioManagerBase::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
}

void AudioManagerBase::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(stream);
  // TODO(xians) : Have a clearer destruction path for the AudioOutputStream.
  // For example, pass the ownership to AudioManager so it can delete the
  // streams.
  --num_output_streams_;
  delete stream;
}

void AudioManagerBase::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(stream);
  // TODO(xians) : Have a clearer destruction path for the AudioInputStream.
  --num_input_streams_;
  delete stream;
}

void AudioManagerBase::IncreaseActiveInputStreamCount() {
  base::AtomicRefCountInc(&num_active_input_streams_);
}

void AudioManagerBase::DecreaseActiveInputStreamCount() {
  DCHECK(IsRecordingInProcess());
  base::AtomicRefCountDec(&num_active_input_streams_);
}

bool AudioManagerBase::IsRecordingInProcess() {
  return !base::AtomicRefCountIsZero(&num_active_input_streams_);
}

void AudioManagerBase::Shutdown() {
  // To avoid running into deadlocks while we stop the thread, shut it down
  // via a local variable while not holding the audio thread lock.
  scoped_ptr<base::Thread> audio_thread;
  {
    base::AutoLock lock(audio_thread_lock_);
    audio_thread_.swap(audio_thread);
  }

  if (!audio_thread.get())
    return;

  CHECK_NE(MessageLoop::current(), audio_thread->message_loop());

  // We must use base::Unretained since Shutdown might have been called from
  // the destructor and we can't alter the refcount of the object at that point.
  audio_thread->message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioManagerBase::ShutdownOnAudioThread,
      base::Unretained(this)));

  // Stop() will wait for any posted messages to be processed first.
  audio_thread->Stop();
}

void AudioManagerBase::ShutdownOnAudioThread() {
// IOS implements audio input only.
#if defined(OS_IOS)
  return;
#else
  // This should always be running on the audio thread, but since we've cleared
  // the audio_thread_ member pointer when we get here, we can't verify exactly
  // what thread we're running on.  The method is not public though and only
  // called from one place, so we'll leave it at that.
  AudioOutputDispatchersMap::iterator it = output_dispatchers_.begin();
  for (; it != output_dispatchers_.end(); ++it) {
    scoped_refptr<AudioOutputDispatcher>& dispatcher = (*it).second;
    if (dispatcher) {
      dispatcher->Shutdown();
      // All AudioOutputProxies must have been freed before Shutdown is called.
      // If they still exist, things will go bad.  They have direct pointers to
      // both physical audio stream objects that belong to the dispatcher as
      // well as the message loop of the audio thread that will soon go away.
      // So, better crash now than later.
      DCHECK(dispatcher->HasOneRef()) << "AudioOutputProxies are still alive";
      dispatcher = NULL;
    }
  }

  output_dispatchers_.clear();
#endif  // defined(OS_IOS)
}

AudioParameters AudioManagerBase::GetPreferredLowLatencyOutputStreamParameters(
    const AudioParameters& input_params) {
#if defined(OS_IOS)
  // IOS implements audio input only.
  NOTIMPLEMENTED();
  return AudioParameters();
#else
  // TODO(dalecurtis): This should include bits per channel and channel layout
  // eventually.
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      input_params.channel_layout(), input_params.input_channels(),
      GetAudioHardwareSampleRate(), 16, GetAudioHardwareBufferSize());
#endif  // defined(OS_IOS)
}

void AudioManagerBase::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  output_listeners_.AddObserver(listener);
}

void AudioManagerBase::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  output_listeners_.RemoveObserver(listener);
}

void AudioManagerBase::NotifyAllOutputDeviceChangeListeners() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DVLOG(1) << "Firing OnDeviceChange() notifications.";
  FOR_EACH_OBSERVER(AudioDeviceListener, output_listeners_, OnDeviceChange());
}

}  // namespace media
