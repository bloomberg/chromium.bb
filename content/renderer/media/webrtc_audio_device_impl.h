// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/renderer/media/audio_device.h"
#include "content/renderer/media/audio_input_device.h"
#include "third_party/webrtc/modules/audio_device/main/interface/audio_device.h"

// A WebRtcAudioDeviceImpl instance implements the abstract interface
// webrtc::AudioDeviceModule which makes it possible for a user (e.g. webrtc::
// VoiceEngine) to register this class as an external AudioDeviceModule.
// The user can then call WebRtcAudioDeviceImpl::StartPlayout() and
// WebRtcAudioDeviceImpl::StartRecording() from the render process
// to initiate and start audio rendering and capturing in the browser process.
// IPC is utilized to set up the media streams.
//
// Usage example (30ms packet size, PCM mono samples at 48kHz sample rate):
//
//   using namespace webrtc;
//
//   scoped_ptr<WebRtcAudioDeviceImpl> external_adm;
//   external_adm.reset(
//       new WebRtcAudioDeviceImpl(1440, 1440, 1, 1, 48000, 48000));
//   VoiceEngine* voe = VoiceEngine::Create();
//   VoEBase* base = VoEBase::GetInterface(voe);
//   base->RegisterAudioDeviceModule(*external_adm);
//   base->Init();
//   int ch = base->CreateChannel();
//   ...
//   base->StartReceive(ch)
//   base->StartPlayout(ch);
//   base->StartSending(ch);
//   ...
//   <== full-duplex audio session ==>
//   ...
//   base->DeleteChannel(ch);
//   base->Terminate();
//   base->Release();
//   VoiceEngine::Delete(voe);
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
//  This class must be created on the main render thread since it creates
//  AudioDevice and AudioInputDevice objects and they both require a valid
//  RenderThread::current() pointer.
//
class WebRtcAudioDeviceImpl
    : public webrtc::AudioDeviceModule,
      public AudioDevice::RenderCallback,
      public AudioInputDevice::CaptureCallback {
 public:
  WebRtcAudioDeviceImpl(size_t input_buffer_size,
                        size_t output_buffer_size,
                        int input_channels,
                        int output_channels,
                        double input_sample_rate,
                        double output_sample_rate);
  virtual ~WebRtcAudioDeviceImpl();

  // AudioDevice::RenderCallback implementation.
  virtual void Render(const std::vector<float*>& audio_data,
                      size_t number_of_frames,
                      size_t audio_delay_milliseconds);

  // AudioInputDevice::CaptureCallback implementation.
  virtual void Capture(const std::vector<float*>& audio_data,
                       size_t number_of_frames,
                       size_t audio_delay_milliseconds);

  // webrtc::Module implementation.
  virtual int32_t Version(char* version,
                          uint32_t& remaining_buffer_in_bytes,
                          uint32_t& position) const;
  virtual int32_t ChangeUniqueId(const int32_t id);
  virtual int32_t TimeUntilNextProcess();
  virtual int32_t Process();

  // webrtc::AudioDeviceModule implementation.
  virtual int32_t ActiveAudioLayer(AudioLayer* audio_layer) const;
  virtual ErrorCode LastError() const;

  virtual int32_t RegisterEventObserver(
      webrtc::AudioDeviceObserver* event_callback);
  virtual int32_t RegisterAudioCallback(webrtc::AudioTransport* audio_callback);

  virtual int32_t Init();
  virtual int32_t Terminate();
  virtual bool Initialized() const;

  virtual int16_t PlayoutDevices();
  virtual int16_t RecordingDevices();
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[webrtc::kAdmMaxDeviceNameSize],
                                    char guid[webrtc::kAdmMaxGuidSize]);
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[webrtc::kAdmMaxDeviceNameSize],
                                      char guid[webrtc::kAdmMaxGuidSize]);

  virtual int32_t SetPlayoutDevice(uint16_t index);
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device);
  virtual int32_t SetRecordingDevice(uint16_t index);
  virtual int32_t SetRecordingDevice(WindowsDeviceType device);

  virtual int32_t PlayoutIsAvailable(bool* available);
  virtual int32_t InitPlayout();
  virtual bool PlayoutIsInitialized() const;
  virtual int32_t RecordingIsAvailable(bool* available);
  virtual int32_t InitRecording();
  virtual bool RecordingIsInitialized() const;

  virtual int32_t StartPlayout();
  virtual int32_t StopPlayout();
  virtual bool Playing() const;
  virtual int32_t StartRecording();
  virtual int32_t StopRecording();
  virtual bool Recording() const;

  virtual int32_t SetAGC(bool enable);
  virtual bool AGC() const;

  virtual int32_t SetWaveOutVolume(uint16_t volume_left,
                                   uint16_t volume_right);
  virtual int32_t WaveOutVolume(uint16_t* volume_left,
                                uint16_t* volume_right) const;

  virtual int32_t SpeakerIsAvailable(bool* available);
  virtual int32_t InitSpeaker();
  virtual bool SpeakerIsInitialized() const;
  virtual int32_t MicrophoneIsAvailable(bool* available);
  virtual int32_t InitMicrophone();
  virtual bool MicrophoneIsInitialized() const;

  virtual int32_t SpeakerVolumeIsAvailable(bool* available);
  virtual int32_t SetSpeakerVolume(uint32_t volume);
  virtual int32_t SpeakerVolume(uint32_t* volume) const;
  virtual int32_t MaxSpeakerVolume(uint32_t* max_volume) const;
  virtual int32_t MinSpeakerVolume(uint32_t* min_volume) const;
  virtual int32_t SpeakerVolumeStepSize(uint16_t* step_size) const;

  virtual int32_t MicrophoneVolumeIsAvailable(bool* available);
  virtual int32_t SetMicrophoneVolume(uint32_t volume);
  virtual int32_t MicrophoneVolume(uint32_t* volume) const;
  virtual int32_t MaxMicrophoneVolume(uint32_t* max_volume) const;
  virtual int32_t MinMicrophoneVolume(uint32_t* min_volume) const;
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* step_size) const;

  virtual int32_t SpeakerMuteIsAvailable(bool* available);
  virtual int32_t SetSpeakerMute(bool enable);
  virtual int32_t SpeakerMute(bool* enabled) const;

  virtual int32_t MicrophoneMuteIsAvailable(bool* available);
  virtual int32_t SetMicrophoneMute(bool enable);
  virtual int32_t MicrophoneMute(bool* enabled) const;

  virtual int32_t MicrophoneBoostIsAvailable(bool* available);
  virtual int32_t SetMicrophoneBoost(bool enable);
  virtual int32_t MicrophoneBoost(bool* enabled) const;

  virtual int32_t StereoPlayoutIsAvailable(bool* available) const;
  virtual int32_t SetStereoPlayout(bool enable);
  virtual int32_t StereoPlayout(bool* enabled) const;
  virtual int32_t StereoRecordingIsAvailable(bool* available) const;
  virtual int32_t SetStereoRecording(bool enable);
  virtual int32_t StereoRecording(bool* enabled) const;
  virtual int32_t SetRecordingChannel(const ChannelType channel);
  virtual int32_t RecordingChannel(ChannelType* channel) const;

  virtual int32_t SetPlayoutBuffer(const BufferType type, uint16_t size_ms);
  virtual int32_t PlayoutBuffer(BufferType* type, uint16_t* size_ms) const;
  virtual int32_t PlayoutDelay(uint16_t* delay_ms) const;
  virtual int32_t RecordingDelay(uint16_t* delay_ms) const;

  virtual int32_t CPULoad(uint16_t* load) const;

  virtual int32_t StartRawOutputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]);
  virtual int32_t StopRawOutputFileRecording();
  virtual int32_t StartRawInputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]);
  virtual int32_t StopRawInputFileRecording();

  virtual int32_t SetRecordingSampleRate(const uint32_t samples_per_sec);
  virtual int32_t RecordingSampleRate(uint32_t* samples_per_sec) const;
  virtual int32_t SetPlayoutSampleRate(const uint32_t samples_per_sec);
  virtual int32_t PlayoutSampleRate(uint32_t* samples_per_sec) const;

  virtual int32_t ResetAudioDevice();
  virtual int32_t SetLoudspeakerStatus(bool enable);
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const;

  // Helpers.
  bool BufferSizeIsValid(size_t buffer_size, float sample_rate) const;

  // Accessors.
  size_t input_buffer_size() const { return input_buffer_size_; }
  size_t output_buffer_size() const { return output_buffer_size_; }
  int input_channels() const { return input_channels_; }
  int output_channels() const { return output_channels_; }

 private:
  // Provides access to the native audio input layer in the browser process.
  scoped_refptr<AudioInputDevice> audio_input_device_;

  // Provides access to the native audio output layer in the browser process.
  scoped_refptr<AudioDevice> audio_output_device_;

  // Weak reference to the audio callback.
  // The webrtc client defines |audio_transport_callback_| by calling
  // RegisterAudioCallback().
  webrtc::AudioTransport* audio_transport_callback_;

  webrtc::AudioDeviceModule::ErrorCode last_error_;

  size_t input_buffer_size_;
  size_t output_buffer_size_;
  int input_channels_;
  int output_channels_;
  double input_sample_rate_;
  double output_sample_rate_;

  int bytes_per_sample_;

  bool initialized_;
  bool playing_;
  bool recording_;

  // Cached value of the current audio delay on the input/capture side.
  int input_delay_ms_;

  // Cached value of the current audio delay on the output/renderer side.
  int output_delay_ms_;

  base::TimeTicks last_process_time_;

  // Buffers used for temporary storage during capture/render callbacks.
  // Allocated during construction to save stack.
  scoped_array<int16> input_buffer_;
  scoped_array<int16> output_buffer_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioDeviceImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
