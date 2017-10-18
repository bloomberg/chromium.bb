// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/stream_mixer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"
#include "chromecast/media/cma/backend/filter_group.h"
#include "chromecast/media/cma/backend/mixer_output_stream.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_impl.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_parser.h"
#include "chromecast/media/cma/backend/stream_mixer_input_impl.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"
#include "media/audio/audio_device_description.h"

#define RUN_ON_MIXER_THREAD(callback, ...)              \
  if (!mixer_task_runner_->BelongsToCurrentThread()) {  \
    POST_TASK_TO_MIXER_THREAD(callback, ##__VA_ARGS__); \
    return;                                             \
  }

#define POST_TASK_TO_MIXER_THREAD(task, ...) \
  mixer_task_runner_->PostTask(              \
      FROM_HERE, base::Bind(task, base::Unretained(this), ##__VA_ARGS__));

namespace chromecast {
namespace media {

namespace {

const int kNumInputChannels = 2;

// The number of frames of silence to write (to prevent underrun) when no inputs
// are present.
const int kPreventUnderrunChunkSize = 512;
const int kDefaultCheckCloseTimeoutMs = 2000;

// The minimum amount of data that we allow in the output buffer before starting
// to skip inputs with no available data.
constexpr base::TimeDelta kMinBufferedData =
    base::TimeDelta::FromMilliseconds(20);

// Resample all audio below this frequency.
const unsigned int kLowSampleRateCutoff = 32000;

// Sample rate to fall back if input sample rate is below kLowSampleRateCutoff.
const unsigned int kLowSampleRateFallback = 48000;

const int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();

const int kUseDefaultFade = -1;
const int kMediaDuckFadeMs = 150;
const int kMediaUnduckFadeMs = 700;
const int kDefaultFilterFrameAlignment = 256;

bool IsOutputDeviceId(const std::string& device) {
  return device == ::media::AudioDeviceDescription::kDefaultDeviceId ||
         device == ::media::AudioDeviceDescription::kCommunicationsDeviceId ||
         device == kLocalAudioDeviceId || device == kAlarmAudioDeviceId ||
         device == kTtsAudioDeviceId;
}

class StreamMixerInstance : public StreamMixer {
 public:
  StreamMixerInstance() {}
  ~StreamMixerInstance() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamMixerInstance);
};

base::LazyInstance<StreamMixerInstance>::DestructorAtExit g_mixer_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

float StreamMixer::VolumeInfo::GetEffectiveVolume() {
  return std::min(volume, limit);
}

// static
bool StreamMixer::single_threaded_for_test_ = false;

// static
StreamMixer* StreamMixer::Get() {
  return g_mixer_instance.Pointer();
}

// static
void StreamMixer::MakeSingleThreadedForTest() {
  single_threaded_for_test_ = true;
  StreamMixer::Get()->ResetTaskRunnerForTest();
}

StreamMixer::StreamMixer()
    : post_processing_pipeline_factory_(
          std::make_unique<PostProcessingPipelineFactoryImpl>()),
      mixer_thread_(new base::Thread("CMA mixer thread")),
      state_(kStateUninitialized),
      retry_write_frames_timer_(new base::Timer(false, false)),
      check_close_timeout_(kDefaultCheckCloseTimeoutMs),
      check_close_timer_(new base::Timer(false, false)),
      filter_frame_alignment_(kDefaultFilterFrameAlignment) {
  if (single_threaded_for_test_) {
    mixer_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  } else {
    base::Thread::Options options;
    options.priority = base::ThreadPriority::REALTIME_AUDIO;
    mixer_thread_->StartWithOptions(options);
    mixer_task_runner_ = mixer_thread_->task_runner();
  }

  num_output_channels_ = GetSwitchValueNonNegativeInt(
      switches::kAudioOutputChannels, kNumInputChannels);
  DCHECK(num_output_channels_ == kNumInputChannels ||
         num_output_channels_ == 1);

  low_sample_rate_cutoff_ =
      chromecast::GetSwitchValueBoolean(switches::kAlsaEnableUpsampling, false)
          ? kLowSampleRateCutoff
          : MixerOutputStream::kInvalidSampleRate;

  // Read post-processing configuration file
  PostProcessingPipelineParser pipeline_parser;

  CreatePostProcessors(&pipeline_parser);

  // TODO(jyw): command line flag for filter frame alignment.
  DCHECK_EQ(filter_frame_alignment_ & (filter_frame_alignment_ - 1), 0)
      << "Alignment must be a power of 2.";

  // --accept-resource-provider should imply a check close timeout of 0.
  int default_close_timeout = chromecast::GetSwitchValueBoolean(
                                  switches::kAcceptResourceProvider, false)
                                  ? 0
                                  : kDefaultCheckCloseTimeoutMs;
  check_close_timeout_ = GetSwitchValueInt(switches::kAlsaCheckCloseTimeout,
                                           default_close_timeout);
}

std::unique_ptr<FilterGroup> StreamMixer::CreateFilterGroup(
    bool mix_to_mono,
    const std::string& name,
    const base::ListValue* filter_list,
    const std::unordered_set<std::string>& device_ids,
    const std::vector<FilterGroup*>& mixed_inputs) {
  auto pipeline = post_processing_pipeline_factory_->CreatePipeline(
      name, filter_list, kNumInputChannels);
  return std::make_unique<FilterGroup>(kNumInputChannels, mix_to_mono, name,
                                       std::move(pipeline), device_ids,
                                       mixed_inputs);
}

void StreamMixer::CreatePostProcessors(
    PostProcessingPipelineParser* pipeline_parser) {
  std::unordered_set<std::string> used_streams;
  for (auto& stream_pipeline : pipeline_parser->GetStreamPipelines()) {
    const auto& device_ids = stream_pipeline.stream_types;
    for (const std::string& stream_type : device_ids) {
      CHECK(IsOutputDeviceId(stream_type))
          << stream_type << " is not a stream type. Stream types are listed "
          << "in chromecast/media/base/audio_device_ids.cc and "
          << "media/audio/audio_device_description.cc";
      CHECK(used_streams.insert(stream_type).second)
          << "Multiple instances of stream type '" << stream_type << "' in "
          << kCastAudioJsonFilePath << ".";
    }
    filter_groups_.push_back(CreateFilterGroup(
        false /* mono_mixer */, *device_ids.begin() /* name */,
        stream_pipeline.pipeline, device_ids,
        std::vector<FilterGroup*>() /* mixed_inputs */));
    if (device_ids.find(::media::AudioDeviceDescription::kDefaultDeviceId) !=
        device_ids.end()) {
      default_filter_ = filter_groups_.back().get();
    }
  }

  // Always provide a default filter; OEM may only specify mix filter.
  if (!default_filter_) {
    std::string kDefaultDeviceId =
        ::media::AudioDeviceDescription::kDefaultDeviceId;
    filter_groups_.push_back(CreateFilterGroup(
        false /* mono_mixer */, kDefaultDeviceId /* name */, nullptr,
        std::unordered_set<std::string>({kDefaultDeviceId}),
        std::vector<FilterGroup*>() /* mixed_inputs */));
    default_filter_ = filter_groups_.back().get();
  }

  std::vector<FilterGroup*> filter_group_ptrs(filter_groups_.size());
  std::transform(
      filter_groups_.begin(), filter_groups_.end(), filter_group_ptrs.begin(),
      [](const std::unique_ptr<FilterGroup>& group) { return group.get(); });

  // Enable Mono mixer in |mix_filter_| if necessary.
  bool enabled_mono_mixer = (num_output_channels_ == 1);
  filter_groups_.push_back(CreateFilterGroup(
      enabled_mono_mixer, "mix", pipeline_parser->GetMixPipeline(),
      std::unordered_set<std::string>() /* device_ids */, filter_group_ptrs));
  mix_filter_ = filter_groups_.back().get();

  filter_groups_.push_back(
      CreateFilterGroup(false /* mono_mixer */, "linearize",
                        pipeline_parser->GetLinearizePipeline(),
                        std::unordered_set<std::string>() /* device_ids */,
                        std::vector<FilterGroup*>({mix_filter_})));
  linearize_filter_ = filter_groups_.back().get();
}

void StreamMixer::ResetTaskRunnerForTest() {
  mixer_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void StreamMixer::ResetPostProcessorsForTest(
    std::unique_ptr<PostProcessingPipelineFactory> pipeline_factory,
    const std::string& pipeline_json) {
  LOG(INFO) << __FUNCTION__ << " disregard previous PostProcessor messages.";
  post_processing_pipeline_factory_ = std::move(pipeline_factory);
  filter_groups_.clear();
  default_filter_ = nullptr;
  PostProcessingPipelineParser parser(pipeline_json);
  CreatePostProcessors(&parser);
}

StreamMixer::~StreamMixer() {
  FinalizeOnMixerThread();
  mixer_thread_->Stop();
  mixer_task_runner_ = nullptr;
}

void StreamMixer::FinalizeOnMixerThread() {
  RUN_ON_MIXER_THREAD(&StreamMixer::FinalizeOnMixerThread);
  Close();

  // Post a task to allow any pending input deletions to run.
  POST_TASK_TO_MIXER_THREAD(&StreamMixer::FinishFinalize);
}

void StreamMixer::FinishFinalize() {
  retry_write_frames_timer_.reset();
  check_close_timer_.reset();
  inputs_.clear();
  ignored_inputs_.clear();
}

bool StreamMixer::Start() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());

  int requested_sample_rate = requested_output_samples_per_second_;

  if (!output_)
    output_ = MixerOutputStream::Create();

  if (low_sample_rate_cutoff_ != MixerOutputStream::kInvalidSampleRate &&
      requested_sample_rate < low_sample_rate_cutoff_) {
    requested_sample_rate =
        output_samples_per_second_ != MixerOutputStream::kInvalidSampleRate
            ? output_samples_per_second_
            : kLowSampleRateFallback;
  }

  if (!output_->Start(requested_sample_rate, num_output_channels_)) {
    SignalError();
    return false;
  }

  output_samples_per_second_ = output_->GetSampleRate();

  // Initialize filters.
  for (auto&& filter_group : filter_groups_) {
    filter_group->Initialize(output_samples_per_second_);
  }

  state_ = kStateNormalPlayback;
  return true;
}

void StreamMixer::Stop() {
  for (auto* observer : loopback_observers_) {
    observer->OnLoopbackInterrupted();
  }

  output_->Stop();

  state_ = kStateUninitialized;
  output_samples_per_second_ = MixerOutputStream::kInvalidSampleRate;
}

void StreamMixer::Close() {
  Stop();
}

void StreamMixer::SignalError() {
  state_ = kStateError;
  retry_write_frames_timer_->Stop();
  for (auto&& input : inputs_) {
    input->SignalError(StreamMixerInput::MixerError::kInternalError);
    ignored_inputs_.push_back(std::move(input));
  }
  inputs_.clear();
  POST_TASK_TO_MIXER_THREAD(&StreamMixer::Close);
}

void StreamMixer::SetMixerOutputStreamForTest(
    std::unique_ptr<MixerOutputStream> output) {
  if (output_) {
    Close();
  }

  output_ = std::move(output);
}

void StreamMixer::WriteFramesForTest() {
  RUN_ON_MIXER_THREAD(&StreamMixer::WriteFramesForTest);
  WriteFrames();
}

void StreamMixer::ClearInputsForTest() {
  RUN_ON_MIXER_THREAD(&StreamMixer::ClearInputsForTest);
  inputs_.clear();
}

void StreamMixer::AddInput(std::unique_ptr<InputQueue> input) {
  RUN_ON_MIXER_THREAD(&StreamMixer::AddInput, base::Passed(std::move(input)));
  DCHECK(input);

  // If the new input is a primary one, we may need to change the output
  // sample rate to match its input sample rate.
  // We only change the output rate if it is not set to a fixed value.
  if (input->primary() && output_ && !output_->IsFixedSampleRate()) {
    CheckChangeOutputRate(input->input_samples_per_second());
  }

  auto type = input->content_type();
  if (input->primary()) {
    input->SetContentTypeVolume(volume_info_[type].GetEffectiveVolume(),
                                kUseDefaultFade);
  } else {
    input->SetContentTypeVolume(volume_info_[type].volume, kUseDefaultFade);
  }
  input->SetMuted(volume_info_[type].muted);

  check_close_timer_->Stop();
  switch (state_) {
    case kStateUninitialized:
      requested_output_samples_per_second_ = input->input_samples_per_second();
      if (!Start()) {
        // Initialization failed.
        ignored_inputs_.push_back(std::move(input));
        return;
      }

    // Fallthrough intended
    case kStateNormalPlayback: {
      bool found_filter_group = false;
      input->Initialize(output_->GetRenderingDelay());
      for (auto&& filter_group : filter_groups_) {
        if (filter_group->CanProcessInput(input.get())) {
          found_filter_group = true;
          input->set_filter_group(filter_group.get());
          LOG(INFO) << "Added input of type " << input->device_id() << " to "
                    << filter_group->name();
          break;
        }
      }

      // Fallback to default_filter_ if provided
      if (!found_filter_group && default_filter_) {
        found_filter_group = true;
        input->set_filter_group(default_filter_);
        LOG(INFO) << "Added input of type " << input->device_id() << " to "
                  << default_filter_->name();
      }

      CHECK(found_filter_group)
          << "Could not find a filter group for " << input->device_id() << "\n"
          << "(consider adding a 'default' processor)";
      inputs_.push_back(std::move(input));
    } break;
    case kStateError:
      input->SignalError(StreamMixerInput::MixerError::kInternalError);
      ignored_inputs_.push_back(std::move(input));
      break;
    default:
      NOTREACHED();
  }
}

void StreamMixer::CheckChangeOutputRate(int input_samples_per_second) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (state_ != kStateNormalPlayback ||
      input_samples_per_second == requested_output_samples_per_second_ ||
      input_samples_per_second == output_samples_per_second_ ||
      input_samples_per_second < static_cast<int>(low_sample_rate_cutoff_)) {
    return;
  }

  for (auto&& input : inputs_) {
    if (input->primary() && !input->IsDeleting()) {
      return;
    }
  }

  // Move all current inputs to the ignored list
  for (auto&& input : inputs_) {
    LOG(INFO) << "Mixer input " << input.get()
              << " now being ignored due to output sample rate change from "
              << output_samples_per_second_ << " to "
              << input_samples_per_second;
    input->SignalError(StreamMixerInput::MixerError::kInputIgnored);
    ignored_inputs_.push_back(std::move(input));
  }
  inputs_.clear();

  requested_output_samples_per_second_ = input_samples_per_second;

  // Restart the output so that the new output sample rate takes effect.
  Stop();
  Start();
}

void StreamMixer::RemoveInput(InputQueue* input) {
  RUN_ON_MIXER_THREAD(&StreamMixer::RemoveInput, input);
  DCHECK(input);
  DCHECK(!input->IsDeleting());
  input->PrepareToDelete(
      base::Bind(&StreamMixer::DeleteInputQueue, base::Unretained(this)));
}

void StreamMixer::DeleteInputQueue(InputQueue* input) {
  // Always post a task, in case an input calls this while we are iterating
  // through the |inputs_| list.
  POST_TASK_TO_MIXER_THREAD(&StreamMixer::DeleteInputQueueInternal, input);
}

void StreamMixer::DeleteInputQueueInternal(InputQueue* input) {
  DCHECK(input);
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  auto match_input = [input](const std::unique_ptr<InputQueue>& item) {
    return item.get() == input;
  };
  auto it = std::find_if(inputs_.begin(), inputs_.end(), match_input);
  if (it == inputs_.end()) {
    it = std::find_if(ignored_inputs_.begin(), ignored_inputs_.end(),
                      match_input);
    DCHECK(it != ignored_inputs_.end());
    ignored_inputs_.erase(it);
  } else {
    inputs_.erase(it);
  }

  if (inputs_.empty()) {
    // Never close if timeout is negative
    if (check_close_timeout_ >= 0) {
      check_close_timer_->Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(check_close_timeout_),
          base::Bind(&StreamMixer::CheckClose, base::Unretained(this)));
    }
  }
}

void StreamMixer::CheckClose() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(inputs_.empty());
  retry_write_frames_timer_->Stop();
  Close();
}

void StreamMixer::OnFramesQueued() {
  if (state_ != kStateNormalPlayback) {
    return;
  }

  if (retry_write_frames_timer_->IsRunning()) {
    return;
  }

  retry_write_frames_timer_->Start(
      FROM_HERE, base::TimeDelta(),
      base::Bind(&StreamMixer::WriteFrames, base::Unretained(this)));
}

void StreamMixer::WriteFrames() {
  retry_write_frames_timer_->Stop();
  if (TryWriteFrames()) {
    retry_write_frames_timer_->Start(
        FROM_HERE, base::TimeDelta(),
        base::Bind(&StreamMixer::WriteFrames, base::Unretained(this)));
  }
}

bool StreamMixer::TryWriteFrames() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK_GE(filter_groups_.size(), 1u);

  if (state_ != kStateNormalPlayback) {
    return false;
  }

  int chunk_size =
      (output_samples_per_second_ * kMaxAudioWriteTimeMilliseconds / 1000) &
      ~(filter_frame_alignment_ - 1);
  bool is_silence = true;
  for (auto&& filter_group : filter_groups_) {
    filter_group->ClearActiveInputs();
  }
  for (auto&& input : inputs_) {
    int read_size = input->MaxReadSize() & ~(filter_frame_alignment_ - 1);
    if (read_size > 0) {
      DCHECK(input->filter_group());
      input->filter_group()->AddActiveInput(input.get());
      chunk_size = std::min(chunk_size, read_size);
      is_silence = false;
    } else if (input->primary()) {
      base::TimeDelta time_until_underrun;
      if (!output_->GetTimeUntilUnderrun(&time_until_underrun)) {
        return false;
      }

      if (time_until_underrun < kMinBufferedData) {
        // If there has been (or soon will be) an underrun, continue without the
        // empty primary input stream.
        input->OnSkipped();
        continue;
      }

      // A primary input cannot provide any data, so wait until later.
      retry_write_frames_timer_->Start(
          FROM_HERE, kMinBufferedData / 2,
          base::Bind(&StreamMixer::WriteFrames, base::Unretained(this)));
      return false;
    } else {
      input->OnSkipped();
    }
  }

  if (is_silence) {
    // No inputs have any data to provide. Push silence to prevent underrun.
    chunk_size = std::max(kPreventUnderrunChunkSize, filter_frame_alignment_);
  }

  DCHECK_EQ(chunk_size % filter_frame_alignment_, 0);
  // Recursively mix and filter each group.
  linearize_filter_->MixAndFilter(chunk_size);

  WriteMixedPcm(chunk_size);
  return true;
}

void StreamMixer::WriteMixedPcm(int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());

  int64_t expected_playback_time;
  MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay =
      output_->GetRenderingDelay();
  if (rendering_delay.timestamp_microseconds == kNoTimestamp) {
    expected_playback_time = kNoTimestamp;
  } else {
    expected_playback_time = rendering_delay.timestamp_microseconds +
                             rendering_delay.delay_microseconds +
                             linearize_filter_->GetRenderingDelayMicroseconds();
  }

  // Downmix reference signal to mono to reduce CPU load.
  int mix_channel_count = mix_filter_->GetOutputChannelCount();

  if (num_output_channels_ == 1 && mix_channel_count != num_output_channels_) {
    for (int i = 0; i < frames; ++i) {
      float sum = 0;
      for (int c = 0; c < mix_channel_count; ++c) {
        sum += mix_filter_->interleaved()[i * mix_channel_count + c];
      }
      mix_filter_->interleaved()[i] = sum / mix_channel_count;
    }
  }

  // Hard limit to [1.0, -1.0]
  for (int i = 0; i < frames * num_output_channels_; ++i) {
    mix_filter_->interleaved()[i] =
        std::min(1.0f, std::max(-1.0f, mix_filter_->interleaved()[i]));
  }

  for (CastMediaShlib::LoopbackAudioObserver* observer : loopback_observers_) {
    observer->OnLoopbackAudio(
        expected_playback_time, kSampleFormatF32, output_samples_per_second_,
        num_output_channels_,
        reinterpret_cast<uint8_t*>(mix_filter_->interleaved()),
        static_cast<size_t>(frames) * num_output_channels_ * sizeof(float));
  }

  // Drop extra channels from linearize filter if necessary.
  int linearize_channel_count = linearize_filter_->GetOutputChannelCount();
  if (num_output_channels_ == 1 &&
      linearize_channel_count != num_output_channels_) {
    for (int i = 0; i < frames; ++i) {
      linearize_filter_->interleaved()[i] =
          linearize_filter_->interleaved()[i * linearize_channel_count];
    }
  }

  // Hard limit to [1.0, -1.0].
  for (int i = 0; i < frames * num_output_channels_; ++i) {
    linearize_filter_->interleaved()[i] =
        std::min(1.0f, std::max(-1.0f, linearize_filter_->interleaved()[i]));
  }

  bool playback_interrupted = false;
  output_->Write(linearize_filter_->interleaved(),
                 frames * num_output_channels_, &playback_interrupted);

  if (playback_interrupted) {
    for (auto* observer : loopback_observers_) {
      observer->OnLoopbackInterrupted();
    }
  }

  MediaPipelineBackend::AudioDecoder::RenderingDelay common_rendering_delay =
      output_->GetRenderingDelay();
  common_rendering_delay.delay_microseconds +=
      linearize_filter_->GetRenderingDelayMicroseconds() +
      mix_filter_->GetRenderingDelayMicroseconds();
  for (auto&& input : inputs_) {
    MediaPipelineBackend::AudioDecoder::RenderingDelay stream_rendering_delay =
        common_rendering_delay;
    stream_rendering_delay.delay_microseconds +=
        input->filter_group()->GetRenderingDelayMicroseconds();
    input->AfterWriteFrames(stream_rendering_delay);
  }
}

void StreamMixer::AddLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  RUN_ON_MIXER_THREAD(&StreamMixer::AddLoopbackAudioObserver, observer);
  DCHECK(observer);
  DCHECK(!base::ContainsValue(loopback_observers_, observer));
  loopback_observers_.push_back(observer);
}

void StreamMixer::RemoveLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  RUN_ON_MIXER_THREAD(&StreamMixer::RemoveLoopbackAudioObserver, observer);
  DCHECK(base::ContainsValue(loopback_observers_, observer));
  loopback_observers_.erase(std::remove(loopback_observers_.begin(),
                                        loopback_observers_.end(), observer),
                            loopback_observers_.end());
  observer->OnRemoved();
}

void StreamMixer::SetVolume(AudioContentType type, float level) {
  RUN_ON_MIXER_THREAD(&StreamMixer::SetVolume, type, level);
  volume_info_[type].volume = level;
  float effective_volume = volume_info_[type].GetEffectiveVolume();
  for (auto&& input : inputs_) {
    if (input->content_type() == type) {
      if (input->primary()) {
        input->SetContentTypeVolume(effective_volume, kUseDefaultFade);
      } else {
        // Volume limits don't apply to effects streams.
        input->SetContentTypeVolume(level, kUseDefaultFade);
      }
    }
  }
}

void StreamMixer::SetMuted(AudioContentType type, bool muted) {
  RUN_ON_MIXER_THREAD(&StreamMixer::SetMuted, type, muted);
  volume_info_[type].muted = muted;
  for (auto&& input : inputs_) {
    if (input->content_type() == type) {
      input->SetMuted(muted);
    }
  }
}

void StreamMixer::SetOutputLimit(AudioContentType type, float limit) {
  RUN_ON_MIXER_THREAD(&StreamMixer::SetOutputLimit, type, limit);
  LOG(INFO) << "Set volume limit for " << static_cast<int>(type) << " to "
            << limit;
  volume_info_[type].limit = limit;
  float effective_volume = volume_info_[type].GetEffectiveVolume();
  int fade_ms = kUseDefaultFade;
  if (type == AudioContentType::kMedia) {
    if (limit >= 1.0f) {  // Unducking.
      fade_ms = kMediaUnduckFadeMs;
    } else {
      fade_ms = kMediaDuckFadeMs;
    }
  }
  for (auto&& input : inputs_) {
    // Volume limits don't apply to effects streams.
    if (input->primary() && input->content_type() == type) {
      input->SetContentTypeVolume(effective_volume, fade_ms);
    }
  }
}

void StreamMixer::SetPostProcessorConfig(const std::string& name,
                                         const std::string& config) {
  RUN_ON_MIXER_THREAD(&StreamMixer::SetPostProcessorConfig, name, config);
  for (auto&& filter_group : filter_groups_) {
    filter_group->SetPostProcessorConfig(name, config);
  }
}

void StreamMixer::UpdatePlayoutChannel(int playout_channel) {
  RUN_ON_MIXER_THREAD(&StreamMixer::UpdatePlayoutChannel, playout_channel);
  LOG(INFO) << "Update playout channel: " << playout_channel;
  DCHECK(mix_filter_);
  DCHECK(linearize_filter_);

  mix_filter_->SetMixToMono(num_output_channels_ == 1 &&
                            playout_channel == kChannelAll);
  linearize_filter_->UpdatePlayoutChannel(playout_channel);
}

void StreamMixer::SetFilterFrameAlignmentForTest(int filter_frame_alignment) {
  RUN_ON_MIXER_THREAD(&StreamMixer::SetFilterFrameAlignmentForTest,
                      filter_frame_alignment);
  CHECK((filter_frame_alignment & (filter_frame_alignment - 1)) == 0)
      << "Frame alignment ( " << filter_frame_alignment
      << ") is not a power of two";
  filter_frame_alignment_ = filter_frame_alignment;
}

}  // namespace media
}  // namespace chromecast
