// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_auhal_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/media_switches.h"

namespace media {

static std::ostream& operator<<(std::ostream& os,
                                const AudioStreamBasicDescription& format) {
  os << "sample rate       : " << format.mSampleRate << std::endl
     << "format ID         : " << format.mFormatID << std::endl
     << "format flags      : " << format.mFormatFlags << std::endl
     << "bytes per packet  : " << format.mBytesPerPacket << std::endl
     << "frames per packet : " << format.mFramesPerPacket << std::endl
     << "bytes per frame   : " << format.mBytesPerFrame << std::endl
     << "channels per frame: " << format.mChannelsPerFrame << std::endl
     << "bits per channel  : " << format.mBitsPerChannel;
  return os;
}

static void ZeroBufferList(AudioBufferList* buffer_list) {
  for (size_t i = 0; i < buffer_list->mNumberBuffers; ++i) {
    memset(buffer_list->mBuffers[i].mData,
           0,
           buffer_list->mBuffers[i].mDataByteSize);
  }
}

static void WrapBufferList(AudioBufferList* buffer_list,
                           AudioBus* bus,
                           int frames) {
  DCHECK(buffer_list);
  DCHECK(bus);
  const int channels = bus->channels();
  const int buffer_list_channels = buffer_list->mNumberBuffers;
  DCHECK_EQ(channels, buffer_list_channels);

  // Copy pointers from AudioBufferList.
  for (int i = 0; i < channels; ++i) {
    bus->SetChannelData(
        i, static_cast<float*>(buffer_list->mBuffers[i].mData));
  }

  // Finally set the actual length.
  bus->set_frames(frames);
}

AUHALStream::AUHALStream(
    AudioManagerMac* manager,
    const AudioParameters& params,
    AudioDeviceID device)
    : manager_(manager),
      params_(params),
      input_channels_(params_.input_channels()),
      output_channels_(params_.channels()),
      number_of_frames_(params_.frames_per_buffer()),
      source_(NULL),
      device_(device),
      audio_unit_(0),
      volume_(1),
      hardware_latency_frames_(0),
      stopped_(false),
      input_buffer_list_(NULL) {
  // We must have a manager.
  DCHECK(manager_);

  DVLOG(1) << "Input channels: " << input_channels_;
  DVLOG(1) << "Output channels: " << output_channels_;
  DVLOG(1) << "Sample rate: " << params_.sample_rate();
  DVLOG(1) << "Buffer size: " << number_of_frames_;
}

AUHALStream::~AUHALStream() {
}

bool AUHALStream::Open() {
  // Get the total number of input and output channels that the
  // hardware supports.
  int device_input_channels;
  bool got_input_channels = AudioManagerMac::GetDeviceChannels(
      device_,
      kAudioDevicePropertyScopeInput,
      &device_input_channels);

  int device_output_channels;
  bool got_output_channels = AudioManagerMac::GetDeviceChannels(
      device_,
      kAudioDevicePropertyScopeOutput,
      &device_output_channels);

  // Sanity check the requested I/O channels.
  if (!got_input_channels ||
      input_channels_ < 0 || input_channels_ > device_input_channels) {
    LOG(ERROR) << "AudioDevice does not support requested input channels.";
    return false;
  }

  if (!got_output_channels ||
      output_channels_ <= 0 || output_channels_ > device_output_channels) {
    LOG(ERROR) << "AudioDevice does not support requested output channels.";
    return false;
  }

  // The requested sample-rate must match the hardware sample-rate.
  int sample_rate = AudioManagerMac::HardwareSampleRateForDevice(device_);

  if (sample_rate != params_.sample_rate()) {
    LOG(ERROR) << "Requested sample-rate: " << params_.sample_rate()
               << " must match the hardware sample-rate: " << sample_rate;
    return false;
  }

  CreateIOBusses();

  bool configured = ConfigureAUHAL();
  if (configured)
    hardware_latency_frames_ = GetHardwareLatency();

  return configured;
}

void AUHALStream::Close() {
  if (input_buffer_list_) {
    input_buffer_list_storage_.reset();
    input_buffer_list_ = NULL;
    input_bus_.reset(NULL);
    output_bus_.reset(NULL);
  }

  if (audio_unit_) {
    AudioUnitUninitialize(audio_unit_);
    AudioComponentInstanceDispose(audio_unit_);
  }

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void AUHALStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  if (!audio_unit_) {
    DLOG(ERROR) << "Open() has not been called successfully";
    return;
  }

  stopped_ = false;
  {
    base::AutoLock auto_lock(source_lock_);
    source_ = callback;
  }

  AudioOutputUnitStart(audio_unit_);
}

void AUHALStream::Stop() {
  if (stopped_)
    return;

  AudioOutputUnitStop(audio_unit_);

  base::AutoLock auto_lock(source_lock_);
  source_ = NULL;
  stopped_ = true;
}

void AUHALStream::SetVolume(double volume) {
  volume_ = static_cast<float>(volume);

  // TODO(crogers): set volume property
}

void AUHALStream::GetVolume(double* volume) {
  *volume = volume_;
}

// Pulls on our provider to get rendered audio stream.
// Note to future hackers of this function: Do not add locks which can
// be contended in the middle of stream processing here (starting and stopping
// the stream are ok) because this is running on a real-time thread.
OSStatus AUHALStream::Render(
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* output_time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  TRACE_EVENT0("audio", "AUHALStream::Render");

  if (number_of_frames != number_of_frames_) {
    // This can happen if we've suddenly changed sample-rates.
    // The stream should be stopping very soon.
    //
    // Unfortunately AUAudioInputStream and AUHALStream share the frame
    // size set by kAudioDevicePropertyBufferFrameSize above on a per process
    // basis.  What this means is that the |number_of_frames| value may be
    // larger or smaller than the value set during ConfigureAUHAL().
    // In this case either audio input or audio output will be broken,
    // so just output silence.
    ZeroBufferList(io_data);
    return noErr;
  }

  if (input_channels_ > 0 && input_buffer_list_) {
    // Get the input data.  |input_buffer_list_| is wrapped
    // to point to the data allocated in |input_bus_|.
    OSStatus result = AudioUnitRender(
        audio_unit_,
        flags,
        output_time_stamp,
        1,
        number_of_frames,
        input_buffer_list_);
    if (result != noErr)
      ZeroBufferList(input_buffer_list_);
  }

  // Make |output_bus_| wrap the output AudioBufferList.
  WrapBufferList(io_data, output_bus_.get(), number_of_frames);

  // Update the playout latency.
  double playout_latency_frames = GetPlayoutLatency(output_time_stamp);

  uint32 hardware_pending_bytes = static_cast<uint32>
      ((playout_latency_frames + 0.5) * output_format_.mBytesPerFrame);

  {
    // Render() shouldn't be called except between AudioOutputUnitStart() and
    // AudioOutputUnitStop() calls, but crash reports have shown otherwise:
    // http://crbug.com/178765.  We use |source_lock_| to prevent races and
    // crashes in Render() when |source_| is cleared.
    base::AutoLock auto_lock(source_lock_);
    if (!source_) {
      ZeroBufferList(io_data);
      return noErr;
    }

    // Supply the input data and render the output data.
    source_->OnMoreIOData(
        input_bus_.get(),
        output_bus_.get(),
        AudioBuffersState(0, hardware_pending_bytes));
  }

  return noErr;
}

// AUHAL callback.
OSStatus AUHALStream::InputProc(
    void* user_data,
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* output_time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  // Dispatch to our class method.
  AUHALStream* audio_output =
      static_cast<AUHALStream*>(user_data);
  if (!audio_output)
    return -1;

  return audio_output->Render(
      flags,
      output_time_stamp,
      bus_number,
      number_of_frames,
      io_data);
}

double AUHALStream::GetHardwareLatency() {
  if (!audio_unit_ || device_ == kAudioObjectUnknown) {
    DLOG(WARNING) << "AudioUnit is NULL or device ID is unknown";
    return 0.0;
  }

  // Get audio unit latency.
  Float64 audio_unit_latency_sec = 0.0;
  UInt32 size = sizeof(audio_unit_latency_sec);
  OSStatus result = AudioUnitGetProperty(
      audio_unit_,
      kAudioUnitProperty_Latency,
      kAudioUnitScope_Global,
      0,
      &audio_unit_latency_sec,
      &size);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "Could not get AudioUnit latency";
    return 0.0;
  }

  // Get output audio device latency.
  static const AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyLatency,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  UInt32 device_latency_frames = 0;
  size = sizeof(device_latency_frames);
  result = AudioObjectGetPropertyData(
      device_,
      &property_address,
      0,
      NULL,
      &size,
      &device_latency_frames);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "Could not get audio device latency";
    return 0.0;
  }

  return static_cast<double>((audio_unit_latency_sec *
      output_format_.mSampleRate) + device_latency_frames);
}

double AUHALStream::GetPlayoutLatency(
    const AudioTimeStamp* output_time_stamp) {
  // Ensure mHostTime is valid.
  if ((output_time_stamp->mFlags & kAudioTimeStampHostTimeValid) == 0)
    return 0;

  // Get the delay between the moment getting the callback and the scheduled
  // time stamp that tells when the data is going to be played out.
  UInt64 output_time_ns = AudioConvertHostTimeToNanos(
      output_time_stamp->mHostTime);
  UInt64 now_ns = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

  // Prevent overflow leading to huge delay information; occurs regularly on
  // the bots, probably less so in the wild.
  if (now_ns > output_time_ns)
    return 0;

  double delay_frames = static_cast<double>
      (1e-9 * (output_time_ns - now_ns) * output_format_.mSampleRate);

  return (delay_frames + hardware_latency_frames_);
}

void AUHALStream::CreateIOBusses() {
  if (input_channels_ > 0) {
    // Allocate storage for the AudioBufferList used for the
    // input data from the input AudioUnit.
    // We allocate enough space for with one AudioBuffer per channel.
    size_t buffer_list_size = offsetof(AudioBufferList, mBuffers[0]) +
        (sizeof(AudioBuffer) * input_channels_);
    input_buffer_list_storage_.reset(new uint8[buffer_list_size]);

    input_buffer_list_ =
        reinterpret_cast<AudioBufferList*>(input_buffer_list_storage_.get());
    input_buffer_list_->mNumberBuffers = input_channels_;

    // |input_bus_| allocates the storage for the PCM input data.
    input_bus_ = AudioBus::Create(input_channels_, number_of_frames_);

    // Make the AudioBufferList point to the memory in |input_bus_|.
    UInt32 buffer_size_bytes = input_bus_->frames() * sizeof(Float32);
    for (size_t i = 0; i < input_buffer_list_->mNumberBuffers; ++i) {
      input_buffer_list_->mBuffers[i].mNumberChannels = 1;
      input_buffer_list_->mBuffers[i].mDataByteSize = buffer_size_bytes;
      input_buffer_list_->mBuffers[i].mData = input_bus_->channel(i);
    }
  }

  // The output bus will wrap the AudioBufferList given to us in
  // the Render() callback.
  DCHECK_GT(output_channels_, 0);
  output_bus_ = AudioBus::CreateWrapper(output_channels_);
}

bool AUHALStream::EnableIO(bool enable, UInt32 scope) {
  // See Apple technote for details about the EnableIO property.
  // Note that we use bus 1 for input and bus 0 for output:
  // http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
  UInt32 enable_IO = enable ? 1 : 0;
  OSStatus result = AudioUnitSetProperty(
      audio_unit_,
      kAudioOutputUnitProperty_EnableIO,
      scope,
      (scope == kAudioUnitScope_Input) ? 1 : 0,
      &enable_IO,
      sizeof(enable_IO));
  return (result == noErr);
}

bool AUHALStream::SetStreamFormat(
    AudioStreamBasicDescription* desc,
    int channels,
    UInt32 scope,
    UInt32 element) {
  DCHECK(desc);
  AudioStreamBasicDescription& format = *desc;

  format.mSampleRate = params_.sample_rate();
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked |
      kLinearPCMFormatFlagIsNonInterleaved;
  format.mBytesPerPacket = sizeof(Float32);
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = sizeof(Float32);
  format.mChannelsPerFrame = channels;
  format.mBitsPerChannel = 32;
  format.mReserved = 0;

  OSStatus result = AudioUnitSetProperty(
      audio_unit_,
      kAudioUnitProperty_StreamFormat,
      scope,
      element,
      &format,
      sizeof(format));
  return (result == noErr);
}

bool AUHALStream::ConfigureAUHAL() {
  if (device_ == kAudioObjectUnknown ||
      (input_channels_ == 0 && output_channels_ == 0))
    return false;

  AudioComponentDescription desc = {
      kAudioUnitType_Output,
      kAudioUnitSubType_HALOutput,
      kAudioUnitManufacturer_Apple,
      0,
      0
  };
  AudioComponent comp = AudioComponentFindNext(0, &desc);
  if (!comp)
    return false;

  OSStatus result = AudioComponentInstanceNew(comp, &audio_unit_);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "AudioComponentInstanceNew() failed.";
    return false;
  }

  result = AudioUnitInitialize(audio_unit_);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "AudioUnitInitialize() failed.";
    return false;
  }

  // Enable input and output as appropriate.
  if (!EnableIO(input_channels_ > 0, kAudioUnitScope_Input))
    return false;
  if (!EnableIO(output_channels_ > 0, kAudioUnitScope_Output))
    return false;

  // Set the device to be used with the AUHAL AudioUnit.
  result = AudioUnitSetProperty(
      audio_unit_,
      kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global,
      0,
      &device_,
      sizeof(AudioDeviceID));
  if (result != noErr)
    return false;

  // Set stream formats.
  // See Apple's tech note for details on the peculiar way that
  // inputs and outputs are handled in the AUHAL concerning scope and bus
  // (element) numbers:
  // http://developer.apple.com/library/mac/#technotes/tn2091/_index.html

  if (input_channels_ > 0) {
    if (!SetStreamFormat(&input_format_,
                         input_channels_,
                         kAudioUnitScope_Output,
                         1))
      return false;
  }

  if (output_channels_ > 0) {
    if (!SetStreamFormat(&output_format_,
                         output_channels_,
                         kAudioUnitScope_Input,
                         0))
      return false;
  }

  // Set the buffer frame size.
  // WARNING: Setting this value changes the frame size for all audio units in
  // the current process.  It's imperative that the input and output frame sizes
  // be the same as the frames_per_buffer() returned by
  // GetDefaultOutputStreamParameters().
  // See http://crbug.com/154352 for details.
  UInt32 buffer_size = number_of_frames_;
  result = AudioUnitSetProperty(
      audio_unit_,
      kAudioDevicePropertyBufferFrameSize,
      kAudioUnitScope_Output,
      0,
      &buffer_size,
      sizeof(buffer_size));
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "AudioUnitSetProperty(kAudioDevicePropertyBufferFrameSize) failed.";
    return false;
  }

  // Setup callback.
  AURenderCallbackStruct callback;
  callback.inputProc = InputProc;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      audio_unit_,
      kAudioUnitProperty_SetRenderCallback,
      kAudioUnitScope_Input,
      0,
      &callback,
      sizeof(callback));
  if (result != noErr)
    return false;

  return true;
}

}  // namespace media
