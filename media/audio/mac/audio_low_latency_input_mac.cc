// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_low_latency_input_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_bus.h"
#include "media/base/data_buffer.h"

namespace media {

// Number of blocks of buffers used in the |fifo_|.
const int kNumberOfBlocksBufferInFifo = 2;

// Max length of sequence of TooManyFramesToProcessError errors.
// The stream will be stopped as soon as this time limit is passed.
const int kMaxErrorTimeoutInSeconds = 1;

// A one-shot timer is created and started in Start() and it triggers
// CheckInputStartupSuccess() after this amount of time. UMA stats marked
// Media.Audio.InputStartupSuccessMac is then updated where true is added
// if input callbacks have started, and false otherwise.
const int kInputCallbackStartTimeoutInSeconds = 5;

static std::ostream& operator<<(std::ostream& os,
                                const AudioStreamBasicDescription& format) {
  // The 32-bit integer format.mFormatID is actually a non-terminated 4 byte
  // string. Example: kAudioFormatLinearPCM = 'lpcm'.
  char format_id_string[5];
  // Converts a 32-bit integer from the host’s native byte order to big-endian.
  UInt32 format_id = CFSwapInt32HostToBig(format.mFormatID);
  bcopy(&format_id, format_id_string, 4);
  os << "sample rate       : " << format.mSampleRate << std::endl
     << "format ID         : " << format_id_string << std::endl
     << "format flags      : " << format.mFormatFlags << std::endl
     << "bytes per packet  : " << format.mBytesPerPacket << std::endl
     << "frames per packet : " << format.mFramesPerPacket << std::endl
     << "bytes per frame   : " << format.mBytesPerFrame << std::endl
     << "channels per frame: " << format.mChannelsPerFrame << std::endl
     << "bits per channel  : " << format.mBitsPerChannel << std::endl
     << "reserved          : " << format.mReserved;
  return os;
}

// See "Technical Note TN2091 - Device input using the HAL Output Audio Unit"
// http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
// for more details and background regarding this implementation.

AUAudioInputStream::AUAudioInputStream(AudioManagerMac* manager,
                                       const AudioParameters& input_params,
                                       AudioDeviceID audio_device_id)
    : manager_(manager),
      number_of_frames_(input_params.frames_per_buffer()),
      sink_(nullptr),
      audio_unit_(0),
      input_device_id_(audio_device_id),
      started_(false),
      hardware_latency_frames_(0),
      number_of_channels_in_frame_(0),
      fifo_(input_params.channels(),
            number_of_frames_,
            kNumberOfBlocksBufferInFifo),
      input_callback_is_active_(false),
      start_was_deferred_(false),
      buffer_size_was_changed_(false) {
  DCHECK(manager_);

  // Set up the desired (output) format specified by the client.
  format_.mSampleRate = input_params.sample_rate();
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked |
                         kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = input_params.bits_per_sample();
  format_.mChannelsPerFrame = input_params.channels();
  format_.mFramesPerPacket = 1;  // uncompressed audio
  format_.mBytesPerPacket = (format_.mBitsPerChannel *
                             input_params.channels()) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;
  format_.mReserved = 0;

  DVLOG(1) << "Desired output format: " << format_;

  // Derive size (in bytes) of the buffers that we will render to.
  UInt32 data_byte_size = number_of_frames_ * format_.mBytesPerFrame;
  DVLOG(1) << "Size of data buffer in bytes : " << data_byte_size;

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer.
  audio_data_buffer_.reset(new uint8_t[data_byte_size]);
  audio_buffer_list_.mNumberBuffers = 1;

  AudioBuffer* audio_buffer = audio_buffer_list_.mBuffers;
  audio_buffer->mNumberChannels = input_params.channels();
  audio_buffer->mDataByteSize = data_byte_size;
  audio_buffer->mData = audio_data_buffer_.get();
}

AUAudioInputStream::~AUAudioInputStream() {}

// Obtain and open the AUHAL AudioOutputUnit for recording.
bool AUAudioInputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Verify that we are not already opened.
  if (audio_unit_)
    return false;

  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return false;
  }

  // Start by obtaining an AudioOuputUnit using an AUHAL component description.

  // Description for the Audio Unit we want to use (AUHAL in this case).
  // The kAudioUnitSubType_HALOutput audio unit interfaces to any audio device.
  // The user specifies which audio device to track. The audio unit can do
  // input from the device as well as output to the device. Bus 0 is used for
  // the output side, bus 1 is used to get audio input from the device.
  AudioComponentDescription desc = {
      kAudioUnitType_Output,
      kAudioUnitSubType_HALOutput,
      kAudioUnitManufacturer_Apple,
      0,
      0
  };

  AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
  DCHECK(comp);

  // Get access to the service provided by the specified Audio Unit.
  OSStatus result = AudioComponentInstanceNew(comp, &audio_unit_);
  if (result) {
    HandleError(result);
    return false;
  }

  // Initialize the AUHAL before making any changes or using it. The audio unit
  // will be initialized once more as last operation in this method but that is
  // intentional. This approach is based on a comment in the CAPlayThrough
  // example from Apple, which states that "AUHAL needs to be initialized
  // *before* anything is done to it".
  // TODO(henrika): remove this extra call if we are unable to see any positive
  // effects of it in our UMA stats.
  result = AudioUnitInitialize(audio_unit_);
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Enable IO on the input scope of the Audio Unit.
  // Note that, these changes must be done *before* setting the AUHAL's
  // current device.

  // After creating the AUHAL object, we must enable IO on the input scope
  // of the Audio Unit to obtain the device input. Input must be explicitly
  // enabled with the kAudioOutputUnitProperty_EnableIO property on Element 1
  // of the AUHAL. Because the AUHAL can be used for both input and output,
  // we must also disable IO on the output scope.

  UInt32 enableIO = 1;

  // Enable input on the AUHAL.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input,
                                1,          // input element 1
                                &enableIO,  // enable
                                sizeof(enableIO));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Disable output on the AUHAL.
  enableIO = 0;
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output,
                                0,          // output element 0
                                &enableIO,  // disable
                                sizeof(enableIO));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Next, set the audio device to be the Audio Unit's current device.
  // Note that, devices can only be set to the AUHAL after enabling IO.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global,
                                0,
                                &input_device_id_,
                                sizeof(input_device_id_));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Register the input procedure for the AUHAL.
  // This procedure will be called when the AUHAL has received new data
  // from the input device.
  AURenderCallbackStruct callback;
  callback.inputProc = InputProc;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      audio_unit_, kAudioOutputUnitProperty_SetInputCallback,
      kAudioUnitScope_Global, 0, &callback, sizeof(callback));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Get the stream format for the selected input device and ensure that the
  // sample rate of the selected input device matches the desired (given at
  // construction) sample rate. We should not rely on sample rate conversion
  // in the AUHAL, only *simple* conversions, e.g., 32-bit float to 16-bit
  // signed integer format.
  AudioStreamBasicDescription input_device_format = {0};
  UInt32 property_size = sizeof(input_device_format);
  result = AudioUnitGetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input, 1, &input_device_format,
                                &property_size);
  DVLOG(1) << "Input device format: " << input_device_format;
  if (input_device_format.mSampleRate != format_.mSampleRate) {
    LOG(ERROR)
        << "Input device's sample rate does not match the client's sample rate";
    result = kAudioUnitErr_FormatNotSupported;
    HandleError(result);
    return false;
  }

  // Modify the IO buffer size if not already set correctly for the selected
  // device.
  if (!manager_->MaybeChangeBufferSize(input_device_id_, audio_unit_, 1,
                                       number_of_frames_,
                                       &buffer_size_was_changed_)) {
    result = kAudioUnitErr_FormatNotSupported;
    HandleError(result);
    return false;
  }

  // If |number_of_frames_| is out of range, the closest valid buffer size will
  // be set instead. Check the current setting and log a warning for a non
  // perfect match. Any such mismatch will be compensated for in InputProc().
  if (buffer_size_was_changed_) {
    UInt32 io_buffer_size_frames;
    property_size = sizeof(io_buffer_size_frames);
    result = AudioUnitGetProperty(
        audio_unit_, kAudioDevicePropertyBufferFrameSize,
        kAudioUnitScope_Global, 0, &io_buffer_size_frames, &property_size);
    LOG_IF(WARNING, io_buffer_size_frames != number_of_frames_)
        << "AUHAL is using best match of IO buffer size: "
        << io_buffer_size_frames;
  }

  // Channel mapping should be supported but add a warning just in case.
  // TODO(henrika): perhaps add to UMA stat to track if this can happen.
  DLOG_IF(WARNING,
          input_device_format.mChannelsPerFrame != format_.mChannelsPerFrame)
      << "AUHAL's audio converter must do channel conversion";

  // Set up the the desired (output) format.
  // For obtaining input from a device, the device format is always expressed
  // on the output scope of the AUHAL's Element 1.
  result = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output, 1, &format_,
                                sizeof(format_));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Finally, initialize the audio unit and ensure that it is ready to render.
  // Allocates memory according to the maximum number of audio frames
  // it can produce in response to a single render call.
  result = AudioUnitInitialize(audio_unit_);
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // The hardware latency is fixed and will not change during the call.
  hardware_latency_frames_ = GetHardwareLatency();

  // The master channel is 0, Left and right are channels 1 and 2.
  // And the master channel is not counted in |number_of_channels_in_frame_|.
  number_of_channels_in_frame_ = GetNumberOfChannelsFromStream();

  return true;
}

void AUAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DLOG_IF(ERROR, !audio_unit_) << "Open() has not been called successfully";
  if (started_ || !audio_unit_)
    return;

  // Check if we should defer Start() for http://crbug.com/160920.
  if (manager_->ShouldDeferStreamStart()) {
    start_was_deferred_ = true;
    // Use a cancellable closure so that if Stop() is called before Start()
    // actually runs, we can cancel the pending start.
    deferred_start_cb_.Reset(base::Bind(
        &AUAudioInputStream::Start, base::Unretained(this), callback));
    manager_->GetTaskRunner()->PostDelayedTask(
        FROM_HERE,
        deferred_start_cb_.callback(),
        base::TimeDelta::FromSeconds(
            AudioManagerMac::kStartDelayInSecsForPowerEvents));
    return;
  }

  sink_ = callback;
  last_success_time_ = base::TimeTicks::Now();
  StartAgc();
  OSStatus result = AudioOutputUnitStart(audio_unit_);
  if (result == noErr) {
    started_ = true;
    // For UMA stat purposes, start a one-shot timer which detects when input
    // callbacks starts indicating if input audio recording works as intended.
    // CheckInputStartupSuccess() will check if |input_callback_is_active_| is
    // true when the timer expires. This timer delay is currently set to
    // 5 seconds to avoid false alarms.
    input_callback_timer_.reset(new base::OneShotTimer());
    input_callback_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kInputCallbackStartTimeoutInSeconds), this,
        &AUAudioInputStream::CheckInputStartupSuccess);
  }
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "Failed to start acquiring data";
}

void AUAudioInputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!started_)
    return;
  StopAgc();
  input_callback_timer_.reset();
  OSStatus result = AudioOutputUnitStop(audio_unit_);
  DCHECK_EQ(result, noErr);
  SetInputCallbackIsActive(false);
  started_ = false;
  sink_ = nullptr;
  fifo_.Clear();
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "Failed to stop acquiring data";
}

void AUAudioInputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  if (started_) {
    Stop();
  }
  CloseAudioUnit();
  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

double AUAudioInputStream::GetMaxVolume() {
  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return 0.0;
  }

  // Query if any of the master, left or right channels has volume control.
  for (int i = 0; i <= number_of_channels_in_frame_; ++i) {
    // If the volume is settable, the  valid volume range is [0.0, 1.0].
    if (IsVolumeSettableOnChannel(i))
      return 1.0;
  }

  // Volume control is not available for the audio stream.
  return 0.0;
}

void AUAudioInputStream::SetVolume(double volume) {
  DVLOG(1) << "SetVolume(volume=" << volume << ")";
  DCHECK_GE(volume, 0.0);
  DCHECK_LE(volume, 1.0);

  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return;
  }

  Float32 volume_float32 = static_cast<Float32>(volume);
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyVolumeScalar,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };

  // Try to set the volume for master volume channel.
  if (IsVolumeSettableOnChannel(kAudioObjectPropertyElementMaster)) {
    OSStatus result = AudioObjectSetPropertyData(
        input_device_id_, &property_address, 0, nullptr, sizeof(volume_float32),
        &volume_float32);
    if (result != noErr) {
      DLOG(WARNING) << "Failed to set volume to " << volume_float32;
    }
    return;
  }

  // There is no master volume control, try to set volume for each channel.
  int successful_channels = 0;
  for (int i = 1; i <= number_of_channels_in_frame_; ++i) {
    property_address.mElement = static_cast<UInt32>(i);
    if (IsVolumeSettableOnChannel(i)) {
      OSStatus result = AudioObjectSetPropertyData(
          input_device_id_, &property_address, 0, NULL, sizeof(volume_float32),
          &volume_float32);
      if (result == noErr)
        ++successful_channels;
    }
  }

  DLOG_IF(WARNING, successful_channels == 0)
      << "Failed to set volume to " << volume_float32;

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double AUAudioInputStream::GetVolume() {
  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return 0.0;
  }

  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyVolumeScalar,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };

  if (AudioObjectHasProperty(input_device_id_, &property_address)) {
    // The device supports master volume control, get the volume from the
    // master channel.
    Float32 volume_float32 = 0.0;
    UInt32 size = sizeof(volume_float32);
    OSStatus result =
        AudioObjectGetPropertyData(input_device_id_, &property_address, 0,
                                   nullptr, &size, &volume_float32);
    if (result == noErr)
      return static_cast<double>(volume_float32);
  } else {
    // There is no master volume control, try to get the average volume of
    // all the channels.
    Float32 volume_float32 = 0.0;
    int successful_channels = 0;
    for (int i = 1; i <= number_of_channels_in_frame_; ++i) {
      property_address.mElement = static_cast<UInt32>(i);
      if (AudioObjectHasProperty(input_device_id_, &property_address)) {
        Float32 channel_volume = 0;
        UInt32 size = sizeof(channel_volume);
        OSStatus result =
            AudioObjectGetPropertyData(input_device_id_, &property_address, 0,
                                       nullptr, &size, &channel_volume);
        if (result == noErr) {
          volume_float32 += channel_volume;
          ++successful_channels;
        }
      }
    }

    // Get the average volume of the channels.
    if (successful_channels != 0)
      return static_cast<double>(volume_float32 / successful_channels);
  }

  DLOG(WARNING) << "Failed to get volume";
  return 0.0;
}

bool AUAudioInputStream::IsMuted() {
  // Verify that we have a valid device.
  DCHECK_NE(input_device_id_, kAudioObjectUnknown) << "Device ID is unknown";

  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyMute,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };

  if (!AudioObjectHasProperty(input_device_id_, &property_address)) {
    DLOG(ERROR) << "Device does not support checking master mute state";
    return false;
  }

  UInt32 muted = 0;
  UInt32 size = sizeof(muted);
  OSStatus result = AudioObjectGetPropertyData(
      input_device_id_, &property_address, 0, nullptr, &size, &muted);
  DLOG_IF(WARNING, result != noErr) << "Failed to get mute state";
  return result == noErr && muted != 0;
}

// AUHAL AudioDeviceOutput unit callback
OSStatus AUAudioInputStream::InputProc(void* user_data,
                                       AudioUnitRenderActionFlags* flags,
                                       const AudioTimeStamp* time_stamp,
                                       UInt32 bus_number,
                                       UInt32 number_of_frames,
                                       AudioBufferList* io_data) {
  // Verify that the correct bus is used (Input bus/Element 1)
  DCHECK_EQ(bus_number, static_cast<UInt32>(1));
  AUAudioInputStream* audio_input =
      reinterpret_cast<AUAudioInputStream*>(user_data);
  DCHECK(audio_input);
  if (!audio_input)
    return kAudioUnitErr_InvalidElement;

  // Indicate that input callbacks have started on the internal AUHAL IO
  // thread. The |input_callback_is_active_| member is read from the creating
  // thread when a timer fires once and set to false in Stop() on the same
  // thread. It means that this thread is the only writer of
  // |input_callback_is_active_| once the tread starts and it should therefore
  // be safe to modify.
  audio_input->SetInputCallbackIsActive(true);

  // Update the |mDataByteSize| value in the audio_buffer_list() since
  // |number_of_frames| can be changed on the fly.
  // |mDataByteSize| needs to be exactly mapping to |number_of_frames|,
  // otherwise it will put CoreAudio into bad state and results in
  // AudioUnitRender() returning -50 for the new created stream.
  // We have also seen kAudioUnitErr_TooManyFramesToProcess (-10874) and
  // kAudioUnitErr_CannotDoInCurrentContext (-10863) as error codes.
  // See crbug/428706 for details.
  UInt32 new_size = number_of_frames * audio_input->format_.mBytesPerFrame;
  AudioBuffer* audio_buffer = audio_input->audio_buffer_list()->mBuffers;
  if (new_size != audio_buffer->mDataByteSize) {
    if (new_size > audio_buffer->mDataByteSize) {
      // This can happen if the device is unpluged during recording. We
      // allocate enough memory here to avoid depending on how CoreAudio
      // handles it.
      // See See http://www.crbug.com/434681 for one example when we can enter
      // this scope.
      audio_input->audio_data_buffer_.reset(new uint8_t[new_size]);
      audio_buffer->mData = audio_input->audio_data_buffer_.get();
    }

    // Update the |mDataByteSize| to match |number_of_frames|.
    audio_buffer->mDataByteSize = new_size;
  }

  // Receive audio from the AUHAL from the output scope of the Audio Unit.
  OSStatus result = AudioUnitRender(audio_input->audio_unit(),
                                    flags,
                                    time_stamp,
                                    bus_number,
                                    number_of_frames,
                                    audio_input->audio_buffer_list());
  if (result) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Media.AudioInputCbErrorMac", result);
    OSSTATUS_DLOG(ERROR, result) << "AudioUnitRender() failed ";
    if (result != kAudioUnitErr_TooManyFramesToProcess) {
      audio_input->HandleError(result);
    } else {
      DCHECK(!audio_input->last_success_time_.is_null());
      // We delay stopping the stream for kAudioUnitErr_TooManyFramesToProcess
      // since it has been observed that some USB headsets can cause this error
      // but only for a few initial frames at startup and then then the stream
      // returns to a stable state again. See b/19524368 for details.
      // Instead, we measure time since last valid audio frame and call
      // HandleError() only if a too long error sequence is detected. We do
      // this to avoid ending up in a non recoverable bad core audio state.
      base::TimeDelta time_since_last_success =
          base::TimeTicks::Now() - audio_input->last_success_time_;
      if ((time_since_last_success >
           base::TimeDelta::FromSeconds(kMaxErrorTimeoutInSeconds))) {
        DLOG(ERROR) << "Too long sequence of TooManyFramesToProcess errors!";
        audio_input->HandleError(result);
      }
    }
    return result;
  }
  // Update time of successful call to AudioUnitRender().
  audio_input->last_success_time_ = base::TimeTicks::Now();

  // Deliver recorded data to the consumer as a callback.
  return audio_input->Provide(number_of_frames,
                              audio_input->audio_buffer_list(),
                              time_stamp);
}

OSStatus AUAudioInputStream::Provide(UInt32 number_of_frames,
                                     AudioBufferList* io_data,
                                     const AudioTimeStamp* time_stamp) {
  // Update the capture latency.
  double capture_latency_frames = GetCaptureLatency(time_stamp);

  // The AGC volume level is updated once every second on a separate thread.
  // Note that, |volume| is also updated each time SetVolume() is called
  // through IPC by the render-side AGC.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);

  AudioBuffer& buffer = io_data->mBuffers[0];
  uint8_t* audio_data = reinterpret_cast<uint8_t*>(buffer.mData);
  uint32_t capture_delay_bytes = static_cast<uint32_t>(
      (capture_latency_frames + 0.5) * format_.mBytesPerFrame);
  DCHECK(audio_data);
  if (!audio_data)
    return kAudioUnitErr_InvalidElement;

  // Dynamically increase capacity of the FIFO to handle larger buffers from
  // CoreAudio. This can happen in combination with Apple Thunderbolt Displays
  // when the Display Audio is used as capture source and the cable is first
  // remove and then inserted again.
  // See http://www.crbug.com/434681 for details.
  if (static_cast<int>(number_of_frames) > fifo_.GetUnfilledFrames()) {
    // Derive required increase in number of FIFO blocks. The increase is
    // typically one block.
    const int blocks =
        static_cast<int>((number_of_frames - fifo_.GetUnfilledFrames()) /
                         number_of_frames_) + 1;
    DLOG(WARNING) << "Increasing FIFO capacity by " << blocks << " blocks";
    fifo_.IncreaseCapacity(blocks);
  }

  // Copy captured (and interleaved) data into FIFO.
  fifo_.Push(audio_data, number_of_frames, format_.mBitsPerChannel / 8);

  // Consume and deliver the data when the FIFO has a block of available data.
  while (fifo_.available_blocks()) {
    const AudioBus* audio_bus = fifo_.Consume();
    DCHECK_EQ(audio_bus->frames(), static_cast<int>(number_of_frames_));

    // Compensate the audio delay caused by the FIFO.
    capture_delay_bytes += fifo_.GetAvailableFrames() * format_.mBytesPerFrame;
    sink_->OnData(this, audio_bus, capture_delay_bytes, normalized_volume);
  }

  return noErr;
}

int AUAudioInputStream::HardwareSampleRate() {
  // Determine the default input device's sample-rate.
  AudioDeviceID device_id = kAudioObjectUnknown;
  UInt32 info_size = sizeof(device_id);

  AudioObjectPropertyAddress default_input_device_address = {
    kAudioHardwarePropertyDefaultInputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &default_input_device_address,
                                               0,
                                               0,
                                               &info_size,
                                               &device_id);
  if (result != noErr)
    return 0.0;

  Float64 nominal_sample_rate;
  info_size = sizeof(nominal_sample_rate);

  AudioObjectPropertyAddress nominal_sample_rate_address = {
    kAudioDevicePropertyNominalSampleRate,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  result = AudioObjectGetPropertyData(device_id,
                                      &nominal_sample_rate_address,
                                      0,
                                      0,
                                      &info_size,
                                      &nominal_sample_rate);
  if (result != noErr)
    return 0.0;

  return static_cast<int>(nominal_sample_rate);
}

double AUAudioInputStream::GetHardwareLatency() {
  if (!audio_unit_ || input_device_id_ == kAudioObjectUnknown) {
    DLOG(WARNING) << "Audio unit object is NULL or device ID is unknown";
    return 0.0;
  }

  // Get audio unit latency.
  Float64 audio_unit_latency_sec = 0.0;
  UInt32 size = sizeof(audio_unit_latency_sec);
  OSStatus result = AudioUnitGetProperty(audio_unit_,
                                         kAudioUnitProperty_Latency,
                                         kAudioUnitScope_Global,
                                         0,
                                         &audio_unit_latency_sec,
                                         &size);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "Could not get audio unit latency";

  // Get input audio device latency.
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyLatency,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };
  UInt32 device_latency_frames = 0;
  size = sizeof(device_latency_frames);
  result = AudioObjectGetPropertyData(input_device_id_, &property_address, 0,
                                      nullptr, &size, &device_latency_frames);
  DLOG_IF(WARNING, result != noErr) << "Could not get audio device latency.";

  return static_cast<double>((audio_unit_latency_sec *
      format_.mSampleRate) + device_latency_frames);
}

double AUAudioInputStream::GetCaptureLatency(
    const AudioTimeStamp* input_time_stamp) {
  // Get the delay between between the actual recording instant and the time
  // when the data packet is provided as a callback.
  UInt64 capture_time_ns = AudioConvertHostTimeToNanos(
      input_time_stamp->mHostTime);
  UInt64 now_ns = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
  double delay_frames = static_cast<double>
      (1e-9 * (now_ns - capture_time_ns) * format_.mSampleRate);

  // Total latency is composed by the dynamic latency and the fixed
  // hardware latency.
  return (delay_frames + hardware_latency_frames_);
}

int AUAudioInputStream::GetNumberOfChannelsFromStream() {
  // Get the stream format, to be able to read the number of channels.
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyStreamFormat,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };
  AudioStreamBasicDescription stream_format;
  UInt32 size = sizeof(stream_format);
  OSStatus result = AudioObjectGetPropertyData(
      input_device_id_, &property_address, 0, nullptr, &size, &stream_format);
  if (result != noErr) {
    DLOG(WARNING) << "Could not get stream format";
    return 0;
  }

  return static_cast<int>(stream_format.mChannelsPerFrame);
}

void AUAudioInputStream::HandleError(OSStatus err) {
  NOTREACHED() << "error " << GetMacOSStatusErrorString(err)
               << " (" << err << ")";
  if (sink_)
    sink_->OnError(this);
}

bool AUAudioInputStream::IsVolumeSettableOnChannel(int channel) {
  Boolean is_settable = false;
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyVolumeScalar,
    kAudioDevicePropertyScopeInput,
    static_cast<UInt32>(channel)
  };
  OSStatus result = AudioObjectIsPropertySettable(input_device_id_,
                                                  &property_address,
                                                  &is_settable);
  return (result == noErr) ? is_settable : false;
}

void AUAudioInputStream::SetInputCallbackIsActive(bool enabled) {
  base::subtle::Release_Store(&input_callback_is_active_, enabled);
}

bool AUAudioInputStream::GetInputCallbackIsActive() {
  return (base::subtle::Acquire_Load(&input_callback_is_active_) != false);
}

void AUAudioInputStream::CheckInputStartupSuccess() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Only add UMA stat related to failing input audio for streams where
  // the AGC has been enabled, e.g. WebRTC audio input streams.
  if (started_ && GetAutomaticGainControl()) {
    // Check if we have called Start() and input callbacks have actually
    // started in time as they should. If that is not the case, we have a
    // problem and the stream is considered dead.
    const bool input_callback_is_active = GetInputCallbackIsActive();
    UMA_HISTOGRAM_BOOLEAN("Media.Audio.InputStartupSuccessMac",
                          input_callback_is_active);
    DVLOG(1) << "input_callback_is_active: " << input_callback_is_active;
    if (!input_callback_is_active) {
      // Now when we know that startup has failed for some reason, add extra
      // UMA stats in an attempt to figure out the exact reason.
      AddHistogramsForFailedStartup();
    }
  }
}

void AUAudioInputStream::CloseAudioUnit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "CloseAudioUnit";
  if (!audio_unit_)
    return;
  OSStatus result = AudioUnitUninitialize(audio_unit_);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioUnitUninitialize() failed.";
  result = AudioComponentInstanceDispose(audio_unit_);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioComponentInstanceDispose() failed.";
  audio_unit_ = 0;
}

void AUAudioInputStream::AddHistogramsForFailedStartup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_BOOLEAN("Media.Audio.InputStartWasDeferredMac",
                        start_was_deferred_);
  UMA_HISTOGRAM_BOOLEAN("Media.Audio.InputBufferSizeWasChangedMac",
                        buffer_size_was_changed_);
  UMA_HISTOGRAM_COUNTS_1000("Media.Audio.NumberOfOutputStreamsMac",
                            manager_->output_streams());
  UMA_HISTOGRAM_COUNTS_1000("Media.Audio.NumberOfLowLatencyInputStreamsMac",
                            manager_->low_latency_input_streams());
  UMA_HISTOGRAM_COUNTS_1000("Media.Audio.NumberOfBasicInputStreamsMac",
                            manager_->basic_input_streams());
  // TODO(henrika): this value will currently always report true. It should be
  // fixed when we understand the problem better.
  UMA_HISTOGRAM_BOOLEAN("Media.Audio.AutomaticGainControlMac",
                        GetAutomaticGainControl());
}

}  // namespace media
