// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_STREAM_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_STREAM_MIXER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "chromecast/media/cma/backend/stream_mixer_input.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

class FilterGroup;
class MixerOutputStream;
class PostProcessingPipelineParser;
class PostProcessingPipelineFactory;

// Mixer implementation. The mixer has one or more input queues; these can be
// added/removed at any time. When an input source pushes frames to an input
// queue, the queue should call StreamMixer::WriteFrames(); this causes
// the mixer to attempt to mix and write out as many frames as possible. To do
// this, the mixer determines how many frames can be read from all inputs (ie,
// it gets the maximum number of frames that can be read from each input, and
// uses the minimum value). Assuming that all primary inputs have some data
// available, the calculated number of frames are pulled from each input (maybe
// resampled, if the input's incoming sample rate is not equal to the mixer's
// output sample rate) and written to a MixerOutputStream.
//
// The rendering delay is recalculated after every successful write to the
// output stream. This delay is passed up to the input sources whenever some new
// data is sucessfully added to the input queue (which happens whenever the
// amount of data in the queue drops below the maximum limit, if data is
// pending). Note that the rendering delay is not accurate while the mixer is
// gathering frames to write, so the rendering delay and the queue size for each
// input must be updated atomically after each write is complete (ie, in
// AfterWriteFrames()).
class StreamMixer {
 public:
  // This mixer will pull data from InputQueues which are added to it, mix the
  // data from the streams, and write the mixed data as a single stream to the
  // output. These methods will be called on the mixer thread only.
  class InputQueue {
   public:
    using OnReadyToDeleteCb = base::Callback<void(InputQueue*)>;

    virtual ~InputQueue() {}

    // Returns the sample rate of this stream *before* data is resampled to
    // match the sample rate expected by the mixer. The returned value must be
    // positive.
    virtual int input_samples_per_second() const = 0;

    // Returns true if the stream is primary. Primary streams will be given
    // precedence for sample rates and will dictate when data is polled.
    virtual bool primary() const = 0;

    // Returns a string describing the content type class.
    // Should be from chromecast/media/base/audio_device_ids.h
    // or media/audio/audio_device_description.h
    virtual std::string device_id() const = 0;

    // Returns the content type for volume control.
    virtual AudioContentType content_type() const = 0;

    // Returns true if PrepareToDelete() has been called.
    virtual bool IsDeleting() const = 0;

    // Initializes the InputQueue after the mixer is set up. At this point the
    // input can correctly determine the mixer's output sample rate.
    virtual void Initialize(
        const MediaPipelineBackend::AudioDecoder::RenderingDelay&
            mixer_rendering_delay) = 0;

    // Sets and gets the FilterGroup the InputQueue matches.
    // This is determined at creation to save time matching the InputQueue
    // to a FilterGroup every time it needs to be mixed.
    virtual void set_filter_group(FilterGroup* filter_group) = 0;
    virtual FilterGroup* filter_group() = 0;

    // Returns the maximum number of frames that can be read from this input
    // stream without filling with zeros. This should return 0 if the queue is
    // empty and EOS has not been queued.
    virtual int MaxReadSize() = 0;

    // Pulls data from the input stream. The input stream should populate |dest|
    // with |frames| frames of data to be mixed. The mixer expects data to be
    // at a sample rate of |output_samples_per_second()|, so each input stream
    // should resample as necessary before returning. |frames| is guaranteed to
    // be no larger than the value returned by the most recent call to
    // MaxReadSize(), and |dest->frames()| shall be >= |frames|.
    virtual void GetResampledData(::media::AudioBus* dest, int frames) = 0;

    // Scale |frames| frames at |src| by the current volume (smoothing as
    // needed). Add the scaled result to |dest|.
    // VolumeScaleAccumulate will be called once for each channel of audio
    // present and |repeat_transition| will be true for channels 2 through n.
    // |src| and |dest| should be 16-byte aligned.
    virtual void VolumeScaleAccumulate(bool repeat_transition,
                                       const float* src,
                                       int frames,
                                       float* dest) = 0;

    // Called when this input has been skipped for output due to not having any
    // data available. This indicates that there will be a gap in the playback
    // from this stream.
    virtual void OnSkipped() = 0;

    // This is called for every InputQueue when the mixer writes data to output
    // for any of its input streams.
    virtual void AfterWriteFrames(
        const MediaPipelineBackend::AudioDecoder::RenderingDelay&
            mixer_rendering_delay) = 0;

    // This will be called when a fatal error occurs in the mixer.
    virtual void SignalError(StreamMixerInput::MixerError error) = 0;

    // Notifies the input that it is being removed by the upper layers, and
    // should do whatever is necessary to become ready to delete from the mixer.
    // Once the input is ready to be removed, it should call the supplied
    // |delete_cb|; this should only happen once per input.
    virtual void PrepareToDelete(const OnReadyToDeleteCb& delete_cb) = 0;

    // Sets the multiplier based on this stream's content type. The resulting
    // output volume should be the content type volume * the per-stream volume
    // multiplier. If |fade_ms| is >= 0, the volume change should be faded over
    // that many milliseconds; otherwise, the default fade time should be used.
    virtual void SetContentTypeVolume(float volume, int fade_ms) = 0;

    // Sets whether or not this stream should be muted.
    virtual void SetMuted(bool muted) = 0;

    // Returns the target volume multiplier of the stream. Fading in or out may
    // cause this to be different from the actual multiplier applied in the last
    // buffer. For the actual multiplier applied, use InstantaneousVolume().
    virtual float TargetVolume() = 0;

    // Returns the largest volume multiplier applied to the last buffer
    // retrieved. This differs from TargetVolume() during transients.
    virtual float InstantaneousVolume() = 0;
  };

  enum State {
    kStateUninitialized,
    kStateNormalPlayback,
    kStateError,
  };

  static StreamMixer* Get();
  static void MakeSingleThreadedForTest();

  int output_samples_per_second() const { return output_samples_per_second_; }
  bool empty() const { return inputs_.empty(); }
  State state() const { return state_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return mixer_task_runner_;
  }

  int filter_frame_alignment() const { return filter_frame_alignment_; }

  // Adds an input to the mixer. The input will live at least until
  // RemoveInput(input) is called. Can be called on any thread.
  void AddInput(std::unique_ptr<InputQueue> input);
  // Instructs the mixer to remove an input. The input should not be referenced
  // after this is called. Can be called on any thread.
  void RemoveInput(InputQueue* input);

  // Attempts to write some frames of audio to the output. Must only be called
  // on the mixer thread.
  void OnFramesQueued();

  void ResetPostProcessorsForTest(
      std::unique_ptr<PostProcessingPipelineFactory> pipeline_factory,
      const std::string& pipeline_json);
  void SetMixerOutputStreamForTest(std::unique_ptr<MixerOutputStream> output);
  void WriteFramesForTest();  // Can be called on any thread.
  void ClearInputsForTest();  // Removes all inputs.

  void AddLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);

  void RemoveLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);

  // Sets the volume multiplier for the given content |type|.
  void SetVolume(AudioContentType type, float level);

  // Sets the mute state for the given content |type|.
  void SetMuted(AudioContentType type, bool muted);

  // Sets the volume multiplier limit for the given content |type|.
  void SetOutputLimit(AudioContentType type, float limit);

  // Sends configuration string |config| to processor |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config);

  // Sets filter data alignment, required by some processors.
  // Must be called before audio playback starts.
  void SetFilterFrameAlignmentForTest(int filter_frame_alignment);

  // StreamMixerInputs may request to have one or all channels played on this
  // device. In the event that there are multiple different channels requested,
  // |kChannelAll| takes precidence, and only one other channel type may be
  // requested at a time.
  // When StreamMixerInputs remove themselves, they must clear their request.
  void AddPlayoutChannelRequest(int channel);
  void RemovePlayoutChannelRequest(int channel);

 protected:
  StreamMixer();
  virtual ~StreamMixer();

 private:
  // Contains volume control information for an audio content type.
  struct VolumeInfo {
    float GetEffectiveVolume();

    float volume = 0.0f;
    float limit = 1.0f;
    bool muted = false;
  };

  void ResetTaskRunnerForTest();
  void FinalizeOnMixerThread();
  void FinishFinalize();

  void CreatePostProcessors(PostProcessingPipelineParser* pipeline_parser);

  bool Start();
  void Stop();
  void Close();
  void SignalError();
  void CheckChangeOutputRate(int input_samples_per_second);

  // Deletes an input queue that has finished preparing to delete itself.
  // May be called on any thread.
  void DeleteInputQueue(InputQueue* input);
  // Runs on mixer thread to complete input queue deletion.
  void DeleteInputQueueInternal(InputQueue* input);
  // Called after a timeout period to close the PCM handle if no inputs are
  // present.
  void CheckClose();

  void WriteFrames();
  bool TryWriteFrames();
  void WriteMixedPcm(int frames);
  void UpdateRenderingDelay(int newly_pushed_frames);
  void ResizeBuffersIfNecessary(int chunk_size);

  // Sets active channel in multichannel group.
  void UpdatePlayoutChannel();

  static bool single_threaded_for_test_;

  std::unique_ptr<MixerOutputStream> output_;
  std::unique_ptr<PostProcessingPipelineFactory>
      post_processing_pipeline_factory_;
  std::unique_ptr<base::Thread> mixer_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> mixer_task_runner_;

  int num_output_channels_ = 0;
  int requested_output_samples_per_second_ = 0;
  int output_samples_per_second_ = 0;
  int low_sample_rate_cutoff_ = 0;

  State state_;

  // Stores the number of inputs requesting a PlayoutChannel.
  // Size is kNumChannels - kChannelAll = 2 - -1 = 3.
  std::vector<int> requested_channel_counts_;

  std::vector<std::unique_ptr<InputQueue>> inputs_;
  std::vector<std::unique_ptr<InputQueue>> ignored_inputs_;

  std::unique_ptr<base::Timer> retry_write_frames_timer_;

  int check_close_timeout_;
  std::unique_ptr<base::Timer> check_close_timer_;

  std::vector<std::unique_ptr<FilterGroup>> filter_groups_;
  FilterGroup* default_filter_;
  FilterGroup* mix_filter_;
  FilterGroup* linearize_filter_;

  // Force data to be filtered in multiples of |filter_frame_alignment_| frames.
  // Must be a multiple of 4 for some NEON implementations. Some
  // AudioPostProcessors require stricter alignment conditions.
  int filter_frame_alignment_;

  std::vector<CastMediaShlib::LoopbackAudioObserver*> loopback_observers_;

  std::map<AudioContentType, VolumeInfo> volume_info_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_STREAM_MIXER_H_
