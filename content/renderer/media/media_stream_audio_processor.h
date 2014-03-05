// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_H_

#include "base/atomicops.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/base/audio_converter.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/interface/module_common_types.h"

namespace blink {
class WebMediaConstraints;
}

namespace media {
class AudioBus;
class AudioFifo;
class AudioParameters;
}  // namespace media

namespace webrtc {
class AudioFrame;
class TypingDetection;
}

namespace content {

class RTCMediaConstraints;

// This class owns an object of webrtc::AudioProcessing which contains signal
// processing components like AGC, AEC and NS. It enables the components based
// on the getUserMedia constraints, processes the data and outputs it in a unit
// of 10 ms data chunk.
class CONTENT_EXPORT MediaStreamAudioProcessor :
    public base::RefCountedThreadSafe<MediaStreamAudioProcessor>,
    NON_EXPORTED_BASE(public WebRtcPlayoutDataSource::Sink) {
 public:
  // |playout_data_source| is used to register this class as a sink to the
  // WebRtc playout data for processing AEC. If clients do not enable AEC,
  // |playout_data_source| won't be used.
  MediaStreamAudioProcessor(const media::AudioParameters& source_params,
                            const blink::WebMediaConstraints& constraints,
                            int effects,
                            WebRtcPlayoutDataSource* playout_data_source);

  // Pushes capture data in |audio_source| to the internal FIFO.
  // Called on the capture audio thread.
  void PushCaptureData(media::AudioBus* audio_source);

  // Processes a block of 10 ms data from the internal FIFO and outputs it via
  // |out|. |out| is the address of the pointer that will be pointed to
  // the post-processed data if the method is returning a true. The lifetime
  // of the data represeted by |out| is guaranteed to outlive the method call.
  // That also says *|out| won't change until this method is called again.
  // |new_volume| receives the new microphone volume from the AGC.
  // The new microphoen volume range is [0, 255], and the value will be 0 if
  // the microphone volume should not be adjusted.
  // Returns true if the internal FIFO has at least 10 ms data for processing,
  // otherwise false.
  // |capture_delay|, |volume| and |key_pressed| will be passed to
  // webrtc::AudioProcessing to help processing the data.
  // Called on the capture audio thread.
  bool ProcessAndConsumeData(base::TimeDelta capture_delay,
                             int volume,
                             bool key_pressed,
                             int* new_volume,
                             int16** out);


  // The audio format of the input to the processor.
  const media::AudioParameters& InputFormat() const;

  // The audio format of the output from the processor.
  const media::AudioParameters& OutputFormat() const;

  // Accessor to check if the audio processing is enabled or not.
  bool has_audio_processing() const { return audio_processing_ != NULL; }

 protected:
  friend class base::RefCountedThreadSafe<MediaStreamAudioProcessor>;
  virtual ~MediaStreamAudioProcessor();

 private:
  friend class MediaStreamAudioProcessorTest;

  class MediaStreamAudioConverter;

  // WebRtcPlayoutDataSource::Sink implementation.
  virtual void OnPlayoutData(media::AudioBus* audio_bus,
                             int sample_rate,
                             int audio_delay_milliseconds) OVERRIDE;
  virtual void OnPlayoutDataSourceChanged() OVERRIDE;

  // Helper to initialize the WebRtc AudioProcessing.
  void InitializeAudioProcessingModule(
      const blink::WebMediaConstraints& constraints, int effects);

  // Helper to initialize the capture converter.
  void InitializeCaptureConverter(const media::AudioParameters& source_params);

  // Helper to initialize the render converter.
  void InitializeRenderConverterIfNeeded(int sample_rate,
                                         int number_of_channels,
                                         int frames_per_buffer);

  // Called by ProcessAndConsumeData().
  // Returns the new microphone volume in the range of |0, 255].
  // When the volume does not need to be updated, it returns 0.
  int ProcessData(webrtc::AudioFrame* audio_frame,
                  base::TimeDelta capture_delay,
                  int volume,
                  bool key_pressed);

  // Called when the processor is going away.
  void StopAudioProcessing();

  // Cached value for the render delay latency. This member is accessed by
  // both the capture audio thread and the render audio thread.
  base::subtle::Atomic32 render_delay_ms_;

  // webrtc::AudioProcessing module which does AEC, AGC, NS, HighPass filter,
  // ..etc.
  scoped_ptr<webrtc::AudioProcessing> audio_processing_;

  // Converter used for the down-mixing and resampling of the capture data.
  scoped_ptr<MediaStreamAudioConverter> capture_converter_;

  // AudioFrame used to hold the output of |capture_converter_|.
  webrtc::AudioFrame capture_frame_;

  // Converter used for the down-mixing and resampling of the render data when
  // the AEC is enabled.
  scoped_ptr<MediaStreamAudioConverter> render_converter_;

  // AudioFrame used to hold the output of |render_converter_|.
  webrtc::AudioFrame render_frame_;

  // Data bus to help converting interleaved data to an AudioBus.
  scoped_ptr<media::AudioBus> render_data_bus_;

  // Raw pointer to the WebRtcPlayoutDataSource, which is valid for the
  // lifetime of RenderThread.
  WebRtcPlayoutDataSource* const playout_data_source_;

  // Used to DCHECK that the destructor is called on the main render thread.
  base::ThreadChecker main_thread_checker_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;

  // Used to DCHECK that PushRenderData() is called on the render audio thread.
  base::ThreadChecker render_thread_checker_;

  // Flag to enable the stereo channels mirroring.
  bool audio_mirroring_;

  // Used by the typing detection.
  scoped_ptr<webrtc::TypingDetection> typing_detector_;

  // Result from the typing detection.
  bool typing_detected_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_H_
