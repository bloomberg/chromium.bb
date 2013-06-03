// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_UNIFIED_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_UNIFIED_WIN_H_

#include <Audioclient.h>
#include <MMDeviceAPI.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_fifo.h"
#include "media/base/channel_mixer.h"
#include "media/base/media_export.h"
#include "media/base/multi_channel_resampler.h"

namespace media {

class AudioManagerWin;

// Implementation of AudioOutputStream for Windows using the Core Audio API
// where both capturing and rendering takes place on the same thread to enable
// audio I/O. This class allows arbitrary combinations of input and output
// devices running off different clocks and using different drivers, with
// potentially differing sample-rates.
//
// It is required to first acquire the native sample rate of the selected
// output device and then use the same rate when creating this object.
// The inner operation depends on the input sample rate which is determined
// during construction. Three different main modes are supported:
//
//  1)  input rate == output rate => input side drives output side directly.
//  2)  input rate != output rate => both sides are driven independently by
//      events and a FIFO plus a resampling unit is used to compensate for
//      differences in sample rates between the two sides.
//  3)  input rate == output rate but native buffer sizes are not identical =>
//      same inner functionality as in (2) to compensate for the differences
//      in buffer sizes and also compensate for any potential clock drift
//      between the two devices.
//
// Mode detection is is done at construction and using mode (1) will lead to
// best performance (lower delay and no "varispeed distortion"), i.e., it is
// recommended to use same sample rates for input and output. Mode (2) uses a
// resampler which supports rate adjustments to fine tune for things like
// clock drift and differences in sample rates between different devices.
// Mode (2) - which uses a FIFO and a adjustable multi-channel resampler -
// is also called the varispeed mode and it is used for case (3) as well to
// compensate for the difference in buffer sizes mainly.
// Mode (3) can happen if two different audio devices are used.
// As an example: some devices needs a buffer size of 441 @ 44.1kHz and others
// 448 @ 44.1kHz. This is a rare case and will only happen for sample rates
// which are even multiples of 11025 Hz (11025, 22050, 44100, 88200 etc.).
//
// Implementation notes:
//
//  - Open() can fail if the input and output parameters do not fulfill
//    certain conditions. See source for Open() for more details.
//  - Channel mixing will be performed if the clients asks for a larger
//    number of channels than the native audio layer provides.
//    Example: client wants stereo but audio layer provides mono. In this case
//    upmixing from mono to stereo (1->2) will be done.
//
// TODO(henrika):
//
//  - Add support for exclusive mode.
//  - Add support for KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, i.e., 32-bit float
//    as internal sample-value representation.
//  - Perform fine-tuning for non-matching sample rates to reduce latency.
//
class MEDIA_EXPORT WASAPIUnifiedStream
    : public AudioOutputStream,
      public base::DelegateSimpleThread::Delegate {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  WASAPIUnifiedStream(AudioManagerWin* manager,
                      const AudioParameters& params,
                      const std::string& input_device_id);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~WASAPIUnifiedStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

  bool started() const {
    return audio_io_thread_.get() != NULL;
  }

  // Returns true if input sample rate differs from the output sample rate.
  // A FIFO and a adjustable multi-channel resampler are utilized in this mode.
  bool VarispeedMode() const { return (fifo_ && resampler_); }

 private:
  enum {
    // Time in milliseconds between two successive delay measurements.
    // We save resources by not updating the delay estimates for each capture
    // event (typically 100Hz rate).
    kTimeDiffInMillisecondsBetweenDelayMeasurements = 1000,

    // Max possible FIFO size.
    kFifoSize = 16384,

    // This value was determined empirically for minimum latency while still
    // guarding against FIFO under-runs. The actual target size will be equal
    // to kTargetFifoSafetyFactor * (native input buffer size).
    // TODO(henrika): tune this value for lowest possible latency for all
    // possible sample rate combinations.
    kTargetFifoSafetyFactor = 2
  };

  // Additional initialization required when input and output sample rate
  // differs. Allocates resources for |fifo_|, |resampler_|, |render_event_|,
  // and the |capture_bus_| and configures the |input_format_| structure
  // given the provided input and output audio parameters.
  void DoVarispeedInitialization(const AudioParameters& input_params,
                                 const AudioParameters& output_params);

  // Clears varispeed related components such as the FIFO and the resampler.
  void ResetVarispeed();

  // Builds WAVEFORMATEX structures for input and output based on input and
  // output audio parameters.
  void SetIOFormats(const AudioParameters& input_params,
                    const AudioParameters& output_params);

  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() OVERRIDE;

  // MultiChannelResampler::MultiChannelAudioSourceProvider implementation.
  // Callback for providing more data into the resampler.
  // Only used in varispeed mode, i.e., when input rate != output rate.
  virtual void ProvideInput(int frame_delay, AudioBus* audio_bus);

  // Issues the OnError() callback to the |source_|.
  void HandleError(HRESULT err);

  // Stops and joins the audio thread in case of an error.
  void StopAndJoinThread(HRESULT err);

  // Converts unique endpoint ID to user-friendly device name.
  std::string GetDeviceName(LPCWSTR device_id) const;

  // Called on the audio IO thread for each capture event.
  // Buffers captured audio into a FIFO if varispeed is used or into an audio
  // bus if input and output sample rates are identical.
  void ProcessInputAudio();

  // Called on the audio IO thread for each render event when varispeed is
  // active or for each capture event when varispeed is not used.
  // In varispeed mode, it triggers a resampling callback, which reads from the
  // FIFO, and calls AudioSourceCallback::OnMoreIOData using the resampled
  // input signal and at the same time asks for data to play out.
  // If input and output rates are the same - instead of reading from the FIFO
  // and do resampling - we read directly from the audio bus used to store
  // captured data in ProcessInputAudio.
  void ProcessOutputAudio(IAudioClock* audio_output_clock);

  // Contains the thread ID of the creating thread.
  base::PlatformThreadId creating_thread_id_;

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerWin* manager_;

  // Contains the audio parameter structure provided at construction.
  AudioParameters params_;
  // For convenience, same as in params_.
  int input_channels_;
  int output_channels_;

  // Unique ID of the input device to be opened.
  const std::string input_device_id_;

  // The sharing mode for the streams.
  // Valid values are AUDCLNT_SHAREMODE_SHARED and AUDCLNT_SHAREMODE_EXCLUSIVE
  // where AUDCLNT_SHAREMODE_SHARED is the default.
  AUDCLNT_SHAREMODE share_mode_;

  // Rendering and capturing is driven by this thread (no message loop).
  // All OnMoreIOData() callbacks will be called from this thread.
  scoped_ptr<base::DelegateSimpleThread> audio_io_thread_;

  // Contains the desired audio output format which is set up at construction.
  // It is required to first acquire the native sample rate of the selected
  // output device and then use the same rate when creating this object.
  WAVEFORMATPCMEX output_format_;

  // Contains the native audio input format which is set up at construction
  // if varispeed mode is utilized.
  WAVEFORMATPCMEX input_format_;

  // True when successfully opened.
  bool opened_;

  // Volume level from 0 to 1 used for output scaling.
  double volume_;

  // Size in audio frames of each audio packet where an audio packet
  // is defined as the block of data which the destination is expected to
  // receive in each OnMoreIOData() callback.
  size_t output_buffer_size_frames_;

  // Size in audio frames of each audio packet where an audio packet
  // is defined as the block of data which the source is expected to
  // deliver in each OnMoreIOData() callback.
  size_t input_buffer_size_frames_;

  // Length of the audio endpoint buffer.
  uint32 endpoint_render_buffer_size_frames_;
  uint32 endpoint_capture_buffer_size_frames_;

  // Counts the number of audio frames written to the endpoint buffer.
  uint64 num_written_frames_;

  // Time stamp for last delay measurement.
  base::TimeTicks last_delay_sample_time_;

  // Contains the total (sum of render and capture) delay in milliseconds.
  double total_delay_ms_;

  // Contains the total (sum of render and capture and possibly FIFO) delay
  // in bytes. The update frequency is set by a constant called
  // |kTimeDiffInMillisecondsBetweenDelayMeasurements|.
  int total_delay_bytes_;

  // Pointer to the client that will deliver audio samples to be played out.
  AudioSourceCallback* source_;

  // IMMDevice interfaces which represents audio endpoint devices.
  base::win::ScopedComPtr<IMMDevice> endpoint_render_device_;
  base::win::ScopedComPtr<IMMDevice> endpoint_capture_device_;

  // IAudioClient interfaces which enables a client to create and initialize
  // an audio stream between an audio application and the audio engine.
  base::win::ScopedComPtr<IAudioClient> audio_output_client_;
  base::win::ScopedComPtr<IAudioClient> audio_input_client_;

  // IAudioRenderClient interfaces enables a client to write output
  // data to a rendering endpoint buffer.
  base::win::ScopedComPtr<IAudioRenderClient> audio_render_client_;

  // IAudioCaptureClient interfaces enables a client to read input
  // data from a capturing endpoint buffer.
  base::win::ScopedComPtr<IAudioCaptureClient> audio_capture_client_;

  // The audio engine will signal this event each time a buffer has been
  // recorded.
  base::win::ScopedHandle capture_event_;

  // The audio engine will signal this event each time it needs a new
  // audio buffer to play out.
  // Only utilized in varispeed mode.
  base::win::ScopedHandle render_event_;

  // This event will be signaled when streaming shall stop.
  base::win::ScopedHandle stop_streaming_event_;

  // Container for retrieving data from AudioSourceCallback::OnMoreIOData().
  scoped_ptr<AudioBus> output_bus_;

  // Container for sending data to AudioSourceCallback::OnMoreIOData().
  scoped_ptr<AudioBus> input_bus_;

  // Container for storing output from the channel mixer.
  scoped_ptr<AudioBus> channel_bus_;

  // All members below are only allocated, or used, in varispeed mode:

  // Temporary storage of resampled input audio data.
  scoped_ptr<AudioBus> resampled_bus_;

  // Set to true first time a capture event has been received in varispeed
  // mode.
  bool input_callback_received_;

  // MultiChannelResampler is a multi channel wrapper for SincResampler;
  // allowing high quality sample rate conversion of multiple channels at once.
  scoped_ptr<MultiChannelResampler> resampler_;

  // Resampler I/O ratio.
  double io_sample_rate_ratio_;

  // Used for input to output buffering.
  scoped_ptr<AudioFifo> fifo_;

  // The channel mixer is only created and utilized if number of input channels
  // is larger than the native number of input channels (e.g client wants
  // stereo but the audio device only supports mono).
  scoped_ptr<ChannelMixer> channel_mixer_;

  // The optimal number of frames we'd like to keep in the FIFO at all times.
  int target_fifo_frames_;

  // A running average of the measured delta between actual number of frames
  // in the FIFO versus |target_fifo_frames_|.
  double average_delta_;

  // A varispeed rate scalar which is calculated based on FIFO drift.
  double fifo_rate_compensation_;

  // Set to true when input side signals output side that a new delay
  // estimate is needed.
  bool update_output_delay_;

  // Capture side stores its delay estimate so the sum can be derived in
  // the render side.
  double capture_delay_ms_;

  // TODO(henrika): possibly remove these members once the performance is
  // properly tuned. Only used for off-line debugging.
#ifndef NDEBUG
  enum LogElementNames {
    INPUT_TIME_STAMP,
    NUM_FRAMES_IN_FIFO,
    RESAMPLER_MARGIN,
    RATE_COMPENSATION
  };

  scoped_ptr<int64[]> input_time_stamps_;
  scoped_ptr<int[]> num_frames_in_fifo_;
  scoped_ptr<int[]> resampler_margin_;
  scoped_ptr<double[]> fifo_rate_comps_;
  scoped_ptr<int[]> num_elements_;
  scoped_ptr<int[]> input_params_;
  scoped_ptr<int[]> output_params_;

  FILE* data_file_;
  FILE* param_file_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WASAPIUnifiedStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_UNIFIED_WIN_H_
