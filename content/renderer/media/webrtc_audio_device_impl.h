// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "content/renderer/media/audio_device.h"
#include "content/renderer/media/audio_input_device.h"
#include "third_party/webrtc/modules/audio_device/main/interface/audio_device.h"

// A WebRtcAudioDeviceImpl instance implements the abstract interface
// webrtc::AudioDeviceModule which makes it possible for a user (e.g. webrtc::
// VoiceEngine) to register this class as an external AudioDeviceModule (ADM).
// Then WebRtcAudioDeviceImpl::SetSessionId() needs to be called to set the
// session id that tells which device to use. The user can either get the
// session id from the MediaStream or use a value of 1 (AudioInputDeviceManager
// ::kFakeOpenSessionId), the later will open the default device without going
// through the MediaStream. The user can then call WebRtcAudioDeviceImpl::
// StartPlayout() and WebRtcAudioDeviceImpl::StartRecording() from the render
// process to initiate and start audio rendering and capturing in the browser
// process. IPC is utilized to set up the media streams.
//
// Usage example:
//
//   using namespace webrtc;
//
//   {
//      scoped_refptr<WebRtcAudioDeviceImpl> external_adm;
//      external_adm = new WebRtcAudioDeviceImpl();
//      external_adm->SetSessionId(1);
//      VoiceEngine* voe = VoiceEngine::Create();
//      VoEBase* base = VoEBase::GetInterface(voe);
//      base->Init(external_adm);
//      int ch = base->CreateChannel();
//      ...
//      base->StartReceive(ch)
//      base->StartPlayout(ch);
//      base->StartSending(ch);
//      ...
//      <== full-duplex audio session ==>
//      ...
//      base->DeleteChannel(ch);
//      base->Terminate();
//      base->Release();
//      VoiceEngine::Delete(voe);
//   }
//
// Note that, WebRtcAudioDeviceImpl::RegisterAudioCallback() will
// be called by the webrtc::VoiceEngine::Init() call and the
// webrtc::VoiceEngine is an webrtc::AudioTransport implementation.
// Hence, when the underlying audio layer wants data samples to be played out,
// the AudioDevice::RenderCallback() will be called, which in turn uses the
// registered webrtc::AudioTransport callback and feeds the data to the
// webrtc::VoiceEngine.
//
// The picture below illustrates the media flow on the capture side:
//
//                   .------------------.            .----------------------.
// (Native audio) => | AudioInputStream |-> OnData ->| AudioInputController |-.
//                   .------------------.            .----------------------. |
//                                                                            |
//                               browser process                              |
//   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - (*)
//                               renderer process                             |
//                                                                            |
//      .-------------------------------.             .------------------.    |
//  .---|    WebRtcAudioDeviceImpl      |<- Capture <-| AudioInputDevice | <--.
//  |   .-------------------------------.             .------------------.
//  |
//  |                             .---------------------.
//  .-> RecordedDataIsAvailable ->| webrtc::VoiceEngine | => (encode+transmit)
//                                .---------------------.
//
//  (*) Using SyncSocket for inter-process synchronization with low latency.
//      The actual data is transferred via SharedMemory. IPC is not involved
//      in the actual media transfer.
//
// Implementation notes:
//
//  - This class must be created on the main render thread.
//  - The webrtc::AudioDeviceModule is reference counted.
//  - Recording is currently not supported on Mac OS X.
//
class CONTENT_EXPORT WebRtcAudioDeviceImpl
    : NON_EXPORTED_BASE(public webrtc::AudioDeviceModule),
      public AudioDevice::RenderCallback,
      public AudioInputDevice::CaptureCallback,
      public AudioInputDevice::CaptureEventHandler {
 public:
  // Methods called on main render thread.
  WebRtcAudioDeviceImpl();

  // webrtc::RefCountedModule implementation.
  // The creator must call AddRef() after construction and use Release()
  // to release the reference and delete this object.
  virtual int32_t AddRef() OVERRIDE;
  virtual int32_t Release() OVERRIDE;

  // We need this one to support runnable method tasks.
  static bool ImplementsThreadSafeReferenceCounting() { return true; }

  // AudioDevice::RenderCallback implementation.
  virtual void Render(const std::vector<float*>& audio_data,
                      size_t number_of_frames,
                      size_t audio_delay_milliseconds) OVERRIDE;

  // AudioInputDevice::CaptureCallback implementation.
  virtual void Capture(const std::vector<float*>& audio_data,
                       size_t number_of_frames,
                       size_t audio_delay_milliseconds) OVERRIDE;

  // AudioInputDevice::CaptureEventHandler implementation.
  virtual void OnDeviceStarted(const std::string& device_id);
  virtual void OnDeviceStopped();

  // webrtc::Module implementation.
  virtual int32_t Version(char* version,
                          uint32_t& remaining_buffer_in_bytes,
                          uint32_t& position) const OVERRIDE;
  virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE;
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;

  // webrtc::AudioDeviceModule implementation.
  virtual int32_t ActiveAudioLayer(AudioLayer* audio_layer) const OVERRIDE;
  virtual ErrorCode LastError() const OVERRIDE;

  virtual int32_t RegisterEventObserver(
      webrtc::AudioDeviceObserver* event_callback) OVERRIDE;
  virtual int32_t RegisterAudioCallback(webrtc::AudioTransport* audio_callback)
      OVERRIDE;

  virtual int32_t Init() OVERRIDE;
  virtual int32_t Terminate() OVERRIDE;
  virtual bool Initialized() const OVERRIDE;

  virtual int16_t PlayoutDevices() OVERRIDE;
  virtual int16_t RecordingDevices() OVERRIDE;
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[webrtc::kAdmMaxDeviceNameSize],
                                    char guid[webrtc::kAdmMaxGuidSize])
                                    OVERRIDE;
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[webrtc::kAdmMaxDeviceNameSize],
                                      char guid[webrtc::kAdmMaxGuidSize])
                                      OVERRIDE;
  virtual int32_t SetPlayoutDevice(uint16_t index) OVERRIDE;
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) OVERRIDE;
  virtual int32_t SetRecordingDevice(uint16_t index) OVERRIDE;
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) OVERRIDE;

  virtual int32_t PlayoutIsAvailable(bool* available) OVERRIDE;
  virtual int32_t InitPlayout() OVERRIDE;
  virtual bool PlayoutIsInitialized() const OVERRIDE;
  virtual int32_t RecordingIsAvailable(bool* available) OVERRIDE;
  virtual int32_t InitRecording() OVERRIDE;
  virtual bool RecordingIsInitialized() const OVERRIDE;

  virtual int32_t StartPlayout() OVERRIDE;
  virtual int32_t StopPlayout() OVERRIDE;
  virtual bool Playing() const OVERRIDE;
  virtual int32_t StartRecording() OVERRIDE;
  virtual int32_t StopRecording() OVERRIDE;
  virtual bool Recording() const OVERRIDE;

  virtual int32_t SetAGC(bool enable) OVERRIDE;
  virtual bool AGC() const OVERRIDE;

  virtual int32_t SetWaveOutVolume(uint16_t volume_left,
                                   uint16_t volume_right) OVERRIDE;
  virtual int32_t WaveOutVolume(uint16_t* volume_left,
                                uint16_t* volume_right) const OVERRIDE;

  virtual int32_t SpeakerIsAvailable(bool* available) OVERRIDE;
  virtual int32_t InitSpeaker() OVERRIDE;
  virtual bool SpeakerIsInitialized() const OVERRIDE;
  virtual int32_t MicrophoneIsAvailable(bool* available) OVERRIDE;
  virtual int32_t InitMicrophone() OVERRIDE;
  virtual bool MicrophoneIsInitialized() const OVERRIDE;

  virtual int32_t SpeakerVolumeIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetSpeakerVolume(uint32_t volume) OVERRIDE;
  virtual int32_t SpeakerVolume(uint32_t* volume) const OVERRIDE;
  virtual int32_t MaxSpeakerVolume(uint32_t* max_volume) const OVERRIDE;
  virtual int32_t MinSpeakerVolume(uint32_t* min_volume) const OVERRIDE;
  virtual int32_t SpeakerVolumeStepSize(uint16_t* step_size) const OVERRIDE;

  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetMicrophoneVolume(uint32_t volume) OVERRIDE;
  virtual int32_t MicrophoneVolume(uint32_t* volume) const OVERRIDE;
  virtual int32_t MaxMicrophoneVolume(uint32_t* max_volume) const OVERRIDE;
  virtual int32_t MinMicrophoneVolume(uint32_t* min_volume) const OVERRIDE;
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* step_size) const OVERRIDE;

  virtual int32_t SpeakerMuteIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetSpeakerMute(bool enable) OVERRIDE;
  virtual int32_t SpeakerMute(bool* enabled) const OVERRIDE;

  virtual int32_t MicrophoneMuteIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetMicrophoneMute(bool enable) OVERRIDE;
  virtual int32_t MicrophoneMute(bool* enabled) const OVERRIDE;

  virtual int32_t MicrophoneBoostIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetMicrophoneBoost(bool enable) OVERRIDE;
  virtual int32_t MicrophoneBoost(bool* enabled) const OVERRIDE;

  virtual int32_t StereoPlayoutIsAvailable(bool* available) const OVERRIDE;
  virtual int32_t SetStereoPlayout(bool enable) OVERRIDE;
  virtual int32_t StereoPlayout(bool* enabled) const OVERRIDE;
  virtual int32_t StereoRecordingIsAvailable(bool* available) const OVERRIDE;
  virtual int32_t SetStereoRecording(bool enable) OVERRIDE;
  virtual int32_t StereoRecording(bool* enabled) const OVERRIDE;
  virtual int32_t SetRecordingChannel(const ChannelType channel) OVERRIDE;
  virtual int32_t RecordingChannel(ChannelType* channel) const OVERRIDE;

  virtual int32_t SetPlayoutBuffer(
      const BufferType type, uint16_t size_ms) OVERRIDE;
  virtual int32_t PlayoutBuffer(
      BufferType* type, uint16_t* size_ms) const OVERRIDE;
  virtual int32_t PlayoutDelay(uint16_t* delay_ms) const OVERRIDE;
  virtual int32_t RecordingDelay(uint16_t* delay_ms) const OVERRIDE;

  virtual int32_t CPULoad(uint16_t* load) const OVERRIDE;

  virtual int32_t StartRawOutputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) OVERRIDE;
  virtual int32_t StopRawOutputFileRecording() OVERRIDE;
  virtual int32_t StartRawInputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) OVERRIDE;
  virtual int32_t StopRawInputFileRecording() OVERRIDE;

  virtual int32_t SetRecordingSampleRate(
      const uint32_t samples_per_sec) OVERRIDE;
  virtual int32_t RecordingSampleRate(uint32_t* samples_per_sec) const OVERRIDE;
  virtual int32_t SetPlayoutSampleRate(const uint32_t samples_per_sec) OVERRIDE;
  virtual int32_t PlayoutSampleRate(uint32_t* samples_per_sec) const OVERRIDE;

  virtual int32_t ResetAudioDevice() OVERRIDE;
  virtual int32_t SetLoudspeakerStatus(bool enable) OVERRIDE;
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const OVERRIDE;

  // Sets the session id.
  void SetSessionId(int session_id);

  // Accessors.
  size_t input_buffer_size() const { return input_buffer_size_; }
  size_t output_buffer_size() const { return output_buffer_size_; }
  int input_channels() const { return input_channels_; }
  int output_channels() const { return output_channels_; }
  int input_sample_rate() const { return static_cast<int>(input_sample_rate_); }
  int output_sample_rate() const {
    return static_cast<int>(output_sample_rate_);
  }
  int input_delay_ms() const { return input_delay_ms_; }
  int output_delay_ms() const { return output_delay_ms_; }
  bool initialized() const { return initialized_; }
  bool playing() const { return playing_; }
  bool recording() const { return recording_; }

 private:
  // Make destructor private to ensure that we can only be deleted by Release().
  virtual ~WebRtcAudioDeviceImpl();

  // Methods called on the main render thread ----------------------------------
  // The following methods are tasks posted on the render thread that needs to
  // be executed on that thread.
  void InitOnRenderThread(int32_t* error, base::WaitableEvent* event);

  int ref_count_;

  // Gives access to the message loop of the render thread on which this
  // object is created.
  scoped_refptr<base::MessageLoopProxy> render_loop_;

  // Provides access to the native audio input layer in the browser process.
  scoped_refptr<AudioInputDevice> audio_input_device_;

  // Provides access to the native audio output layer in the browser process.
  scoped_refptr<AudioDevice> audio_output_device_;

  // Weak reference to the audio callback.
  // The webrtc client defines |audio_transport_callback_| by calling
  // RegisterAudioCallback().
  webrtc::AudioTransport* audio_transport_callback_;

  // Cached values of utilized audio parameters. Platform dependent.
  size_t input_buffer_size_;
  size_t output_buffer_size_;
  int input_channels_;
  int output_channels_;
  double input_sample_rate_;
  double output_sample_rate_;

  // Cached value of the current audio delay on the input/capture side.
  int input_delay_ms_;

  // Cached value of the current audio delay on the output/renderer side.
  int output_delay_ms_;

  // Buffers used for temporary storage during capture/render callbacks.
  // Allocated during initialization to save stack.
  scoped_array<int16> input_buffer_;
  scoped_array<int16> output_buffer_;

  webrtc::AudioDeviceModule::ErrorCode last_error_;

  base::TimeTicks last_process_time_;

  // Id of the media session to be started, it tells which device to be used
  // on the input/capture side.
  int session_id_;

  // Protects |recording_|.
  base::Lock lock_;

  int bytes_per_sample_;

  bool initialized_;
  bool playing_;
  bool recording_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioDeviceImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
