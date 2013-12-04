// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_device_not_impl.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_renderer_sink.h"

// A WebRtcAudioDeviceImpl instance implements the abstract interface
// webrtc::AudioDeviceModule which makes it possible for a user (e.g. webrtc::
// VoiceEngine) to register this class as an external AudioDeviceModule (ADM).
// Then WebRtcAudioDeviceImpl::SetSessionId() needs to be called to set the
// session id that tells which device to use. The user can then call
// WebRtcAudioDeviceImpl::StartPlayout() and
// WebRtcAudioDeviceImpl::StartRecording() from the render process to initiate
// and start audio rendering and capturing in the browser process. IPC is
// utilized to set up the media streams.
//
// Usage example:
//
//   using namespace webrtc;
//
//   {
//      scoped_refptr<WebRtcAudioDeviceImpl> external_adm;
//      external_adm = new WebRtcAudioDeviceImpl();
//      external_adm->SetSessionId(session_id);
//      VoiceEngine* voe = VoiceEngine::Create();
//      VoEBase* base = VoEBase::GetInterface(voe);
//      base->Init(external_adm);
//      int ch = base->CreateChannel();
//      ...
//      base->StartReceive(ch)
//      base->StartPlayout(ch);
//      base->StartSending(ch);
//      ...
//      <== full-duplex audio session with AGC enabled ==>
//      ...
//      base->DeleteChannel(ch);
//      base->Terminate();
//      base->Release();
//      VoiceEngine::Delete(voe);
//   }
//
// webrtc::VoiceEngine::Init() calls these ADM methods (in this order):
//
//  RegisterAudioCallback(this)
//    webrtc::VoiceEngine is an webrtc::AudioTransport implementation and
//    implements the RecordedDataIsAvailable() and NeedMorePlayData() callbacks.
//
//  Init()
//    Creates and initializes the AudioOutputDevice and AudioInputDevice
//    objects.
//
//  SetAGC(true)
//    Enables the adaptive analog mode of the AGC which ensures that a
//    suitable microphone volume level will be set. This scheme will affect
//    the actual microphone control slider.
//
// AGC overview:
//
// It aims to maintain a constant speech loudness level from the microphone.
// This is done by both controlling the analog microphone gain and applying
// digital gain. The microphone gain on the sound card is slowly
// increased/decreased during speech only. By observing the microphone control
// slider you can see it move when you speak. If you scream, the slider moves
// downwards and then upwards again when you return to normal. It is not
// uncommon that the slider hits the maximum. This means that the maximum
// analog gain is not large enough to give the desired loudness. Nevertheless,
// we can in general still attain the desired loudness. If the microphone
// control slider is moved manually, the gain adaptation restarts and returns
// to roughly the same position as before the change if the circumstances are
// still the same. When the input microphone signal causes saturation, the
// level is decreased dramatically and has to re-adapt towards the old level.
// The adaptation is a slowly varying process and at the beginning of capture
// this is noticed by a slow increase in volume. Smaller changes in microphone
// input level is leveled out by the built-in digital control. For larger
// differences we need to rely on the slow adaptation.
// See http://en.wikipedia.org/wiki/Automatic_gain_control for more details.
//
// AGC implementation details:
//
// The adaptive analog mode of the AGC is always enabled for desktop platforms
// in WebRTC.
//
// Before recording starts, the ADM enables AGC on the AudioInputDevice.
//
// A capture session with AGC is started up as follows (simplified):
//
//                            [renderer]
//                                |
//                     ADM::StartRecording()
//             AudioInputDevice::InitializeOnIOThread()
//           AudioInputHostMsg_CreateStream(..., agc=true)               [IPC]
//                                |
//                       [IPC to the browser]
//                                |
//              AudioInputRendererHost::OnCreateStream()
//              AudioInputController::CreateLowLatency()
//         AudioInputController::DoSetAutomaticGainControl(true)
//            AudioInputStream::SetAutomaticGainControl(true)
//                                |
// AGC is now enabled in the media layer and streaming starts (details omitted).
// The figure below illustrates the AGC scheme which is active in combination
// with the default media flow explained earlier.
//                                |
//                            [browser]
//                                |
//                AudioInputStream::(Capture thread loop)
//  AgcAudioStream<AudioInputStream>::GetAgcVolume() => get latest mic volume
//                 AudioInputData::OnData(..., volume)
//              AudioInputController::OnData(..., volume)
//               AudioInputSyncWriter::Write(..., volume)
//                                |
//      [volume | size | data] is sent to the renderer         [shared memory]
//                                |
//                            [renderer]
//                                |
//          AudioInputDevice::AudioThreadCallback::Process()
//            WebRtcAudioDeviceImpl::Capture(..., volume)
//    AudioTransport::RecordedDataIsAvailable(...,volume, new_volume)
//                                |
// The AGC now uses the current volume input and computes a suitable new
// level given by the |new_level| output. This value is only non-zero if the
// AGC has take a decision that the microphone level should change.
//                                |
//                      if (new_volume != 0)
//              AudioInputDevice::SetVolume(new_volume)
//              AudioInputHostMsg_SetVolume(new_volume)                  [IPC]
//                                |
//                       [IPC to the browser]
//                                |
//                 AudioInputRendererHost::OnSetVolume()
//                  AudioInputController::SetVolume()
//             AudioInputStream::SetVolume(scaled_volume)
//                                |
// Here we set the new microphone level in the media layer and at the same time
// read the new setting (we might not get exactly what is set).
//                                |
//             AudioInputData::OnData(..., updated_volume)
//           AudioInputController::OnData(..., updated_volume)
//                                |
//                                |
// This process repeats until we stop capturing data. Note that, a common
// steady state is that the volume control reaches its max and the new_volume
// value from the AGC is zero. A loud voice input is required to break this
// state and start lowering the level again.
//
// Implementation notes:
//
//  - This class must be created and destroyed on the main render thread and
//    most methods are called on the same thread. However, some methods are
//    also called on a Libjingle worker thread. RenderData is called on the
//    AudioOutputDevice thread and CaptureData on the AudioInputDevice thread.
//    To summarize: this class lives on four different threads.
//  - The webrtc::AudioDeviceModule is reference counted.
//  - AGC is only supported in combination with the WASAPI-based audio layer
//    on Windows, i.e., it is not supported on Windows XP.
//  - All volume levels required for the AGC scheme are transfered in a
//    normalized range [0.0, 1.0]. Scaling takes place in both endpoints
//    (WebRTC client a media layer). This approach ensures that we can avoid
//    transferring maximum levels between the renderer and the browser.
//

namespace content {

class WebRtcAudioCapturer;
class WebRtcAudioRenderer;

// TODO(xians): Move the following two interfaces to webrtc so that
// libjingle can own references to the renderer and capturer.
class WebRtcAudioRendererSource {
 public:
  // Callback to get the rendered interleaved data.
  // TODO(xians): Change uint8* to int16*.
  virtual void RenderData(uint8* audio_data,
                          int number_of_channels,
                          int number_of_frames,
                          int audio_delay_milliseconds) = 0;

  // Set the format for the capture audio parameters.
  virtual void SetRenderFormat(const media::AudioParameters& params) = 0;

  // Callback to notify the client that the renderer is going away.
  virtual void RemoveAudioRenderer(WebRtcAudioRenderer* renderer) = 0;

 protected:
  virtual ~WebRtcAudioRendererSource() {}
};

class PeerConnectionAudioSink {
 public:
  // Callback to deliver the captured interleaved data.
  // |channels| contains a vector of WebRtc VoE channels.
  // |audio_data| is the pointer to the audio data.
  // |sample_rate| is the sample frequency of audio data.
  // |number_of_channels| is the number of channels reflecting the order of
  // surround sound channels.
  // |audio_delay_milliseconds| is recording delay value.
  // |current_volume| is current microphone volume, in range of |0, 255].
  // |need_audio_processing| indicates if the audio needs WebRtc AEC/NS/AGC
  // audio processing.
  // The return value is the new microphone volume, in the range of |0, 255].
  // When the volume does not need to be updated, it returns 0.
  virtual int OnData(const int16* audio_data,
                     int sample_rate,
                     int number_of_channels,
                     int number_of_frames,
                     const std::vector<int>& channels,
                     int audio_delay_milliseconds,
                     int current_volume,
                     bool need_audio_processing,
                     bool key_pressed) = 0;

  // Set the format for the capture audio parameters.
  // This is called when the capture format has changed, and it must be called
  // on the same thread as calling CaptureData().
  virtual void OnSetFormat(const media::AudioParameters& params) = 0;

 protected:
 virtual ~PeerConnectionAudioSink() {}
};

// Note that this class inherits from webrtc::AudioDeviceModule but due to
// the high number of non-implemented methods, we move the cruft over to the
// WebRtcAudioDeviceNotImpl.
class CONTENT_EXPORT WebRtcAudioDeviceImpl
    : NON_EXPORTED_BASE(public PeerConnectionAudioSink),
      NON_EXPORTED_BASE(public WebRtcAudioDeviceNotImpl),
      NON_EXPORTED_BASE(public WebRtcAudioRendererSource) {
 public:
  // The maximum volume value WebRtc uses.
  static const int kMaxVolumeLevel = 255;

  // Instances of this object are created on the main render thread.
  WebRtcAudioDeviceImpl();

  // webrtc::RefCountedModule implementation.
  // The creator must call AddRef() after construction and use Release()
  // to release the reference and delete this object.
  // Called on the main render thread.
  virtual int32_t AddRef() OVERRIDE;
  virtual int32_t Release() OVERRIDE;

  // webrtc::AudioDeviceModule implementation.
  // All implemented methods are called on the main render thread unless
  // anything else is stated.

  virtual int32_t RegisterAudioCallback(webrtc::AudioTransport* audio_callback)
      OVERRIDE;

  virtual int32_t Init() OVERRIDE;
  virtual int32_t Terminate() OVERRIDE;
  virtual bool Initialized() const OVERRIDE;

  virtual int32_t PlayoutIsAvailable(bool* available) OVERRIDE;
  virtual bool PlayoutIsInitialized() const OVERRIDE;
  virtual int32_t RecordingIsAvailable(bool* available) OVERRIDE;
  virtual bool RecordingIsInitialized() const OVERRIDE;

  // All Start/Stop methods are called on a libJingle worker thread.
  virtual int32_t StartPlayout() OVERRIDE;
  virtual int32_t StopPlayout() OVERRIDE;
  virtual bool Playing() const OVERRIDE;
  virtual int32_t StartRecording() OVERRIDE;
  virtual int32_t StopRecording() OVERRIDE;
  virtual bool Recording() const OVERRIDE;

  // Called on the AudioInputDevice worker thread.
  virtual int32_t SetMicrophoneVolume(uint32_t volume) OVERRIDE;

  // TODO(henrika): sort out calling thread once we start using this API.
  virtual int32_t MicrophoneVolume(uint32_t* volume) const OVERRIDE;

  virtual int32_t MaxMicrophoneVolume(uint32_t* max_volume) const OVERRIDE;
  virtual int32_t MinMicrophoneVolume(uint32_t* min_volume) const OVERRIDE;
  virtual int32_t StereoPlayoutIsAvailable(bool* available) const OVERRIDE;
  virtual int32_t StereoRecordingIsAvailable(bool* available) const OVERRIDE;
  virtual int32_t PlayoutDelay(uint16_t* delay_ms) const OVERRIDE;
  virtual int32_t RecordingDelay(uint16_t* delay_ms) const OVERRIDE;
  virtual int32_t RecordingSampleRate(uint32_t* samples_per_sec) const OVERRIDE;
  virtual int32_t PlayoutSampleRate(uint32_t* samples_per_sec) const OVERRIDE;

  // Sets the |renderer_|, returns false if |renderer_| already exists.
  // Called on the main renderer thread.
  bool SetAudioRenderer(WebRtcAudioRenderer* renderer);

  // Adds the capturer to the ADM.
  void AddAudioCapturer(const scoped_refptr<WebRtcAudioCapturer>& capturer);

  // Gets the default capturer, which is the capturer in the list with
  // a valid |device_id|. Microphones are represented by capturers with a valid
  // |device_id|, since only one microphone is supported today, only one
  // capturer in the |capturers_| can have a valid |device_id|.
  scoped_refptr<WebRtcAudioCapturer> GetDefaultCapturer() const;

  const scoped_refptr<WebRtcAudioRenderer>& renderer() const {
    return renderer_;
  }
  int output_buffer_size() const {
    return output_audio_parameters_.frames_per_buffer();
  }
  int output_channels() const {
    return output_audio_parameters_.channels();
  }
  int output_sample_rate() const {
    return output_audio_parameters_.sample_rate();
  }

 private:
  typedef std::list<scoped_refptr<WebRtcAudioCapturer> > CapturerList;

  // Make destructor private to ensure that we can only be deleted by Release().
  virtual ~WebRtcAudioDeviceImpl();

  // PeerConnectionAudioSink implementation.

  // Called on the AudioInputDevice worker thread.
  virtual int OnData(const int16* audio_data,
                     int sample_rate,
                     int number_of_channels,
                     int number_of_frames,
                     const std::vector<int>& channels,
                     int audio_delay_milliseconds,
                     int current_volume,
                     bool need_audio_processing,
                     bool key_pressed) OVERRIDE;

  // Called on the AudioInputDevice worker thread.
  virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE;

  // WebRtcAudioRendererSource implementation.

  // Called on the AudioInputDevice worker thread.
  virtual void RenderData(uint8* audio_data,
                          int number_of_channels,
                          int number_of_frames,
                          int audio_delay_milliseconds) OVERRIDE;

  // Called on the main render thread.
  virtual void SetRenderFormat(const media::AudioParameters& params) OVERRIDE;
  virtual void RemoveAudioRenderer(WebRtcAudioRenderer* renderer) OVERRIDE;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  int ref_count_;

  // List of captures which provides access to the native audio input layer
  // in the browser process.
  CapturerList capturers_;

  // Provides access to the audio renderer in the browser process.
  scoped_refptr<WebRtcAudioRenderer> renderer_;

  // Weak reference to the audio callback.
  // The webrtc client defines |audio_transport_callback_| by calling
  // RegisterAudioCallback().
  webrtc::AudioTransport* audio_transport_callback_;

  // Cached values of used output audio parameters. Platform dependent.
  media::AudioParameters output_audio_parameters_;

  // Cached value of the current audio delay on the input/capture side.
  int input_delay_ms_;

  // Cached value of the current audio delay on the output/renderer side.
  int output_delay_ms_;

  // Protects |recording_|, |output_delay_ms_|, |input_delay_ms_|, |renderer_|
  // |recording_| and |microphone_volume_|.
  mutable base::Lock lock_;

  bool initialized_;
  bool playing_;
  bool recording_;

  // Used for histograms of total recording and playout times.
  base::Time start_capture_time_;
  base::Time start_render_time_;

  // Stores latest microphone volume received in a CaptureData() callback.
  // Range is [0, 255].
  uint32_t microphone_volume_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
