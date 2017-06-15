// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"

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
#include "base/numerics/saturated_arithmetic.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/cma/backend/alsa/alsa_wrapper.h"
#include "chromecast/media/cma/backend/alsa/filter_group.h"
#include "chromecast/media/cma/backend/alsa/post_processing_pipeline_parser.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa_input_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/media_switches.h"

#define RETURN_REPORT_ERROR(snd_func, ...)                        \
  do {                                                            \
    int err = alsa_->snd_func(__VA_ARGS__);                       \
    if (err < 0) {                                                \
      LOG(ERROR) << #snd_func " error: " << alsa_->StrError(err); \
      SignalError();                                              \
      return;                                                     \
    }                                                             \
  } while (0)

#define RETURN_ERROR_CODE(snd_func, ...)                          \
  do {                                                            \
    int err = alsa_->snd_func(__VA_ARGS__);                       \
    if (err < 0) {                                                \
      LOG(ERROR) << #snd_func " error: " << alsa_->StrError(err); \
      return err;                                                 \
    }                                                             \
  } while (0)

#define CHECK_PCM_INITIALIZED()                                              \
  if (!pcm_ || !pcm_hw_params_) {                                            \
    LOG(WARNING) << __FUNCTION__ << "() called after failed initialization"; \
    return;                                                                  \
  }

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

const char kOutputDeviceDefaultName[] = "default";
const int kNumInputChannels = 2;

const int kDefaultOutputBufferSizeFrames = 4096;
const bool kPcmRecoverIsSilent = false;
// The number of frames of silence to write (to prevent underrun) when no inputs
// are present.
const int kPreventUnderrunChunkSize = 512;
const int kDefaultCheckCloseTimeoutMs = 2000;

const int kMaxWriteSizeMs = 20;
// The minimum amount of data that we allow in the ALSA buffer before starting
// to skip inputs with no available data.
const int kMinBufferedDataMs = 8;

// A list of supported sample rates.
// TODO(jyw): move this up into chromecast/public for 1) documentation and
// 2) to help when implementing IsSampleRateSupported()
// clang-format off
const int kSupportedSampleRates[] =
    { 8000, 11025, 12000,
     16000, 22050, 24000,
     32000, 44100, 48000,
     64000, 88200, 96000};
// clang-format on
const int kInvalidSampleRate = 0;

// Arbitrary sample rate in Hz to mix all audio to when a new primary input has
// a sample rate that is not directly supported, and a better fallback sample
// rate cannot be determined. 48000 is the highest supported non-hi-res sample
// rate. 96000 is the highest supported hi-res sample rate.
const unsigned int kFallbackSampleRate = 48000;
const unsigned int kFallbackSampleRateHiRes = 96000;

// Resample all audio below this frequency.
const unsigned int kLowSampleRateCutoff = 32000;

// The snd_pcm_(hw|sw)_params_set_*_near families of functions will report what
// direction they adjusted the requested parameter in, but since we read the
// output param and then log the information, this module doesn't need to get
// the direction explicitly.
static int* kAlsaDirDontCare = nullptr;

// These sample formats will be tried in order. 32 bit samples is ideal, but
// some devices do not support 32 bit samples.
const snd_pcm_format_t kPreferredSampleFormats[] = {
    SND_PCM_FORMAT_FLOAT, SND_PCM_FORMAT_S32, SND_PCM_FORMAT_S16};

const int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();

const int kUseDefaultFade = -1;
const int kMediaDuckFadeMs = 150;
const int kMediaUnduckFadeMs = 700;

int64_t TimespecToMicroseconds(struct timespec time) {
  return static_cast<int64_t>(time.tv_sec) *
             base::Time::kMicrosecondsPerSecond +
         time.tv_nsec / 1000;
}

bool IsOutputDeviceId(const std::string& device) {
  return device == ::media::AudioDeviceDescription::kDefaultDeviceId ||
         device == ::media::AudioDeviceDescription::kCommunicationsDeviceId ||
         device == kLocalAudioDeviceId || device == kAlarmAudioDeviceId ||
         device == kTtsAudioDeviceId;
}

class StreamMixerAlsaInstance : public StreamMixerAlsa {
 public:
  StreamMixerAlsaInstance() {}
  ~StreamMixerAlsaInstance() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamMixerAlsaInstance);
};

base::LazyInstance<StreamMixerAlsaInstance>::DestructorAtExit g_mixer_instance =
    LAZY_INSTANCE_INITIALIZER;

template <class TargetSampleTypeTraits>
void ToFixedPoint(const float* input,
                  int frames,
                  typename TargetSampleTypeTraits::ValueType* dest_buffer) {
  for (int f = 0; f < frames; ++f) {
    dest_buffer[f] = TargetSampleTypeTraits::FromFloat(input[f]);
  }
}

void ToFixedPoint(const float* input,
                  int frames,
                  int bytes_per_sample,
                  uint8_t* dest_buffer) {
  switch (bytes_per_sample) {
    case 1:
      ToFixedPoint<::media::UnsignedInt8SampleTypeTraits>(
          input, frames, reinterpret_cast<uint8_t*>(dest_buffer));
      break;
    case 2:
      ToFixedPoint<::media::SignedInt16SampleTypeTraits>(
          input, frames, reinterpret_cast<int16_t*>(dest_buffer));
      break;
    case 4:
      ToFixedPoint<::media::SignedInt32SampleTypeTraits>(
          input, frames, reinterpret_cast<int32_t*>(dest_buffer));
      break;
    default:
      NOTREACHED() << "Unsupported bytes per sample encountered: "
                   << bytes_per_sample;
  }
}

}  // namespace

float StreamMixerAlsa::VolumeInfo::GetEffectiveVolume() {
  return std::min(volume, limit);
}

// static
bool StreamMixerAlsa::single_threaded_for_test_ = false;

// static
StreamMixerAlsa* StreamMixerAlsa::Get() {
  return g_mixer_instance.Pointer();
}

// static
void StreamMixerAlsa::MakeSingleThreadedForTest() {
  single_threaded_for_test_ = true;
  StreamMixerAlsa::Get()->ResetTaskRunnerForTest();
}

StreamMixerAlsa::StreamMixerAlsa()
    : mixer_thread_(new base::Thread("ALSA CMA mixer thread")),
      mixer_task_runner_(nullptr),
      requested_output_samples_per_second_(kInvalidSampleRate),
      output_samples_per_second_(kInvalidSampleRate),
      pcm_(nullptr),
      pcm_hw_params_(nullptr),
      pcm_status_(nullptr),
      pcm_format_(SND_PCM_FORMAT_UNKNOWN),
      alsa_buffer_size_(0),
      alsa_period_size_(0),
      alsa_start_threshold_(0),
      alsa_avail_min_(0),
      state_(kStateUninitialized),
      retry_write_frames_timer_(new base::Timer(false, false)),
      check_close_timeout_(kDefaultCheckCloseTimeoutMs),
      check_close_timer_(new base::Timer(false, false)) {
  if (single_threaded_for_test_) {
    mixer_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  } else {
    base::Thread::Options options;
    options.priority = base::ThreadPriority::REALTIME_AUDIO;
    mixer_thread_->StartWithOptions(options);
    mixer_task_runner_ = mixer_thread_->task_runner();
  }

  alsa_device_name_ = kOutputDeviceDefaultName;
  if (base::CommandLine::InitializedForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlsaOutputDevice)) {
    alsa_device_name_ =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAlsaOutputDevice);
  }

  int fixed_samples_per_second = GetSwitchValueNonNegativeInt(
      switches::kAlsaFixedOutputSampleRate, kInvalidSampleRate);
  if (fixed_samples_per_second != kInvalidSampleRate) {
    LOG(INFO) << "Setting fixed sample rate to " << fixed_samples_per_second;
  }

  fixed_output_samples_per_second_ = fixed_samples_per_second;

  num_output_channels_ = GetSwitchValueNonNegativeInt(
      switches::kAudioOutputChannels, kNumInputChannels);
  DCHECK(num_output_channels_ == kNumInputChannels ||
         num_output_channels_ == 1);

  low_sample_rate_cutoff_ =
      chromecast::GetSwitchValueBoolean(switches::kAlsaEnableUpsampling, false)
          ? kLowSampleRateCutoff
          : 0;

  // Read post-processing configuration file
  PostProcessingPipelineParser pipeline_parser;

  CreatePostProcessors(&pipeline_parser);

  // TODO(bshaya): Add support for final mix AudioPostProcessor.
  DefineAlsaParameters();
}

void StreamMixerAlsa::CreatePostProcessors(
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
          << pipeline_parser->GetFilePath() << ".";
    }
    filter_groups_.push_back(base::MakeUnique<FilterGroup>(
        kNumInputChannels, false /* mono_mixer */,
        *device_ids.begin() /* name */, stream_pipeline.pipeline, device_ids,
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
    filter_groups_.push_back(base::MakeUnique<FilterGroup>(
        kNumInputChannels, false /* mono_mixer */, kDefaultDeviceId /* name */,
        nullptr, std::unordered_set<std::string>({kDefaultDeviceId}),
        std::vector<FilterGroup*>() /* mixed_inputs */));
    default_filter_ = filter_groups_.back().get();
  }

  std::vector<FilterGroup*> filter_group_ptrs(filter_groups_.size());
  std::transform(
      filter_groups_.begin(), filter_groups_.end(), filter_group_ptrs.begin(),
      [](const std::unique_ptr<FilterGroup>& group) { return group.get(); });

  // Enable Mono mixer in |mix_filter_| if necessary.
  bool enabled_mono_mixer = (num_output_channels_ == 1);
  filter_groups_.push_back(base::MakeUnique<FilterGroup>(
      kNumInputChannels, enabled_mono_mixer, "mix",
      pipeline_parser->GetMixPipeline(),
      std::unordered_set<std::string>() /* device_ids */, filter_group_ptrs));
  mix_filter_ = filter_groups_.back().get();

  filter_groups_.push_back(base::MakeUnique<FilterGroup>(
      num_output_channels_, false /* mono_mixer */, "linearize",
      pipeline_parser->GetLinearizePipeline(),
      std::unordered_set<std::string>() /* device_ids */,
      std::vector<FilterGroup*>({mix_filter_})));
  linearize_filter_ = filter_groups_.back().get();
}

void StreamMixerAlsa::ResetTaskRunnerForTest() {
  mixer_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void StreamMixerAlsa::ResetPostProcessorsForTest(
    const std::string& pipeline_json) {
  LOG(INFO) << __FUNCTION__ << " disregard previous PostProcessor messages.";
  filter_groups_.clear();
  default_filter_ = nullptr;
  PostProcessingPipelineParser parser(pipeline_json);
  CreatePostProcessors(&parser);
}

void StreamMixerAlsa::DefineAlsaParameters() {
  // Get the ALSA output configuration from the command line.
  alsa_buffer_size_ = GetSwitchValueNonNegativeInt(
      switches::kAlsaOutputBufferSize, kDefaultOutputBufferSizeFrames);

  alsa_period_size_ = GetSwitchValueNonNegativeInt(
      switches::kAlsaOutputPeriodSize, alsa_buffer_size_ / 16);
  if (alsa_period_size_ >= alsa_buffer_size_) {
    LOG(DFATAL) << "ALSA period size must be smaller than the buffer size";
    alsa_period_size_ = alsa_buffer_size_ / 2;
  }

  alsa_start_threshold_ = GetSwitchValueNonNegativeInt(
      switches::kAlsaOutputStartThreshold,
      (alsa_buffer_size_ / alsa_period_size_) * alsa_period_size_);
  if (alsa_start_threshold_ > alsa_buffer_size_) {
    LOG(DFATAL) << "ALSA start threshold must be no larger than "
                << "the buffer size";
    alsa_start_threshold_ =
        (alsa_buffer_size_ / alsa_period_size_) * alsa_period_size_;
  }

  // By default, allow the transfer when at least period_size samples can be
  // processed.
  alsa_avail_min_ = GetSwitchValueNonNegativeInt(switches::kAlsaOutputAvailMin,
                                                 alsa_period_size_);
  if (alsa_avail_min_ > alsa_buffer_size_) {
    LOG(DFATAL) << "ALSA avail min must be no larger than the buffer size";
    alsa_avail_min_ = alsa_period_size_;
  }

  // --accept-resource-provider should imply a check close timeout of 0.
  int default_close_timeout = chromecast::GetSwitchValueBoolean(
                                  switches::kAcceptResourceProvider, false)
                                  ? 0
                                  : kDefaultCheckCloseTimeoutMs;
  check_close_timeout_ = GetSwitchValueInt(switches::kAlsaCheckCloseTimeout,
                                           default_close_timeout);
}

unsigned int StreamMixerAlsa::DetermineOutputRate(unsigned int requested_rate) {
  if (fixed_output_samples_per_second_ != kInvalidSampleRate) {
    LOG(INFO) << "Requested output rate is " << requested_rate;
    LOG(INFO) << "Cannot change rate since it is fixed to "
              << fixed_output_samples_per_second_;
    return fixed_output_samples_per_second_;
  }

  unsigned int unsigned_output_samples_per_second = requested_rate;

  // Try the requested sample rate. If the ALSA driver doesn't know how to deal
  // with it, try the nearest supported sample rate instead. Lastly, try some
  // common sample rates as a fallback. Note that PcmHwParamsSetRateNear
  // doesn't always choose a rate that's actually near the given input sample
  // rate when the input sample rate is not supported.
  const int* kSupportedSampleRatesEnd =
      kSupportedSampleRates + arraysize(kSupportedSampleRates);
  auto* nearest_sample_rate =
      std::min_element(kSupportedSampleRates, kSupportedSampleRatesEnd,
                       [this](int r1, int r2) -> bool {
                         return abs(requested_output_samples_per_second_ - r1) <
                                abs(requested_output_samples_per_second_ - r2);
                       });
  // Resample audio with sample rates deemed to be too low (i.e.  below 32kHz)
  // because some common AV receivers don't support optical out at these
  // frequencies. See b/26385501
  unsigned int first_choice_sample_rate = requested_rate;
  if (requested_rate < low_sample_rate_cutoff_) {
    first_choice_sample_rate = output_samples_per_second_ != kInvalidSampleRate
                                   ? output_samples_per_second_
                                   : kFallbackSampleRate;
  }
  const unsigned int preferred_sample_rates[] = {
      first_choice_sample_rate,
      static_cast<unsigned int>(*nearest_sample_rate),
      kFallbackSampleRateHiRes,
      kFallbackSampleRate};
  int err;
  for (const auto& sample_rate : preferred_sample_rates) {
    err = alsa_->PcmHwParamsTestRate(pcm_, pcm_hw_params_, sample_rate,
                                     0 /* try exact rate */);
    if (err == 0) {
      unsigned_output_samples_per_second = sample_rate;
      break;
    }
  }
  LOG_IF(ERROR, err != 0) << "Even the fallback sample rate isn't supported! "
                          << "Have you tried /bin/alsa_api_test on-device?";
  return unsigned_output_samples_per_second;
}

int StreamMixerAlsa::SetAlsaPlaybackParams() {
  int err = 0;
  // Set hardware parameters.
  DCHECK(pcm_);
  DCHECK(!pcm_hw_params_);
  RETURN_ERROR_CODE(PcmHwParamsMalloc, &pcm_hw_params_);
  RETURN_ERROR_CODE(PcmHwParamsAny, pcm_, pcm_hw_params_);
  RETURN_ERROR_CODE(PcmHwParamsSetAccess, pcm_, pcm_hw_params_,
                    SND_PCM_ACCESS_RW_INTERLEAVED);
  if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
    for (const auto& pcm_format : kPreferredSampleFormats) {
      err = alsa_->PcmHwParamsTestFormat(pcm_, pcm_hw_params_, pcm_format);
      if (err < 0) {
        LOG(WARNING) << "PcmHwParamsTestFormat: " << alsa_->StrError(err);
      } else {
        pcm_format_ = pcm_format;
        break;
      }
    }
    if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
      LOG(ERROR) << "Could not find a valid PCM format. Running "
                 << "/bin/alsa_api_test may be instructive.";
      return err;
    }
  }

  RETURN_ERROR_CODE(PcmHwParamsSetFormat, pcm_, pcm_hw_params_, pcm_format_);
  RETURN_ERROR_CODE(PcmHwParamsSetChannels, pcm_, pcm_hw_params_,
                    num_output_channels_);

  // Set output rate, allow resampling with a warning if the device doesn't
  // support the rate natively.
  RETURN_ERROR_CODE(PcmHwParamsSetRateResample, pcm_, pcm_hw_params_,
                    false /* Don't allow resampling. */);
  unsigned int requested_rate =
      static_cast<unsigned int>(requested_output_samples_per_second_);

  unsigned int unsigned_output_samples_per_second =
      DetermineOutputRate(requested_rate);
  RETURN_ERROR_CODE(PcmHwParamsSetRateNear, pcm_, pcm_hw_params_,
                    &unsigned_output_samples_per_second, kAlsaDirDontCare);
  if (requested_rate != unsigned_output_samples_per_second) {
    LOG(WARNING) << "Requested sample rate (" << requested_rate
                 << " Hz) does not match the actual sample rate ("
                 << unsigned_output_samples_per_second
                 << " Hz). This may lead to lower audio quality.";
  }
  LOG(INFO) << "Sample rate changed from " << output_samples_per_second_
            << " to " << unsigned_output_samples_per_second;
  output_samples_per_second_ =
      static_cast<int>(unsigned_output_samples_per_second);

  snd_pcm_uframes_t requested_buffer_size = alsa_buffer_size_;
  RETURN_ERROR_CODE(PcmHwParamsSetBufferSizeNear, pcm_, pcm_hw_params_,
                    &alsa_buffer_size_);
  if (requested_buffer_size != alsa_buffer_size_) {
    LOG(WARNING) << "Requested buffer size (" << requested_buffer_size
                 << " frames) does not match the actual buffer size ("
                 << alsa_buffer_size_
                 << " frames). This may lead to an increase in "
                    "either audio latency or audio underruns.";

    if (alsa_period_size_ >= alsa_buffer_size_) {
      snd_pcm_uframes_t new_period_size = alsa_buffer_size_ / 2;
      LOG(DFATAL) << "Configured period size (" << alsa_period_size_
                  << ") is >= actual buffer size (" << alsa_buffer_size_
                  << "); reducing to " << new_period_size;
      alsa_period_size_ = new_period_size;
    }
    // Scale the start threshold and avail_min based on the new buffer size.
    float original_buffer_size = static_cast<float>(requested_buffer_size);
    float avail_min_ratio = original_buffer_size / alsa_avail_min_;
    alsa_avail_min_ = alsa_buffer_size_ / avail_min_ratio;
    float start_threshold_ratio = original_buffer_size / alsa_start_threshold_;
    alsa_start_threshold_ = alsa_buffer_size_ / start_threshold_ratio;
  }

  snd_pcm_uframes_t requested_period_size = alsa_period_size_;
  RETURN_ERROR_CODE(PcmHwParamsSetPeriodSizeNear, pcm_, pcm_hw_params_,
                    &alsa_period_size_, kAlsaDirDontCare);
  if (requested_period_size != alsa_period_size_) {
    LOG(WARNING) << "Requested period size (" << requested_period_size
                 << " frames) does not match the actual period size ("
                 << alsa_period_size_
                 << " frames). This may lead to an increase in "
                    "CPU usage or an increase in audio latency.";
  }
  RETURN_ERROR_CODE(PcmHwParams, pcm_, pcm_hw_params_);

  // Set software parameters.
  snd_pcm_sw_params_t* swparams;
  RETURN_ERROR_CODE(PcmSwParamsMalloc, &swparams);
  RETURN_ERROR_CODE(PcmSwParamsCurrent, pcm_, swparams);
  RETURN_ERROR_CODE(PcmSwParamsSetStartThreshold, pcm_, swparams,
                    alsa_start_threshold_);
  if (alsa_start_threshold_ > alsa_buffer_size_) {
    LOG(ERROR) << "Requested start threshold (" << alsa_start_threshold_
               << " frames) is larger than the buffer size ("
               << alsa_buffer_size_
               << " frames). Audio playback will not start.";
  }

  RETURN_ERROR_CODE(PcmSwParamsSetAvailMin, pcm_, swparams, alsa_avail_min_);
  RETURN_ERROR_CODE(PcmSwParamsSetTstampMode, pcm_, swparams,
                    SND_PCM_TSTAMP_ENABLE);
  RETURN_ERROR_CODE(PcmSwParamsSetTstampType, pcm_, swparams,
                    kAlsaTstampTypeMonotonicRaw);
  err = alsa_->PcmSwParams(pcm_, swparams);
  alsa_->PcmSwParamsFree(swparams);
  return err;
}

StreamMixerAlsa::~StreamMixerAlsa() {
  FinalizeOnMixerThread();
  mixer_thread_->Stop();
  mixer_task_runner_ = nullptr;
}

void StreamMixerAlsa::FinalizeOnMixerThread() {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::FinalizeOnMixerThread);
  Close();

  // Post a task to allow any pending input deletions to run.
  POST_TASK_TO_MIXER_THREAD(&StreamMixerAlsa::FinishFinalize);
}

void StreamMixerAlsa::FinishFinalize() {
  retry_write_frames_timer_.reset();
  check_close_timer_.reset();
  inputs_.clear();
  ignored_inputs_.clear();
}

void StreamMixerAlsa::Start() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (!pcm_) {
    RETURN_REPORT_ERROR(PcmOpen, &pcm_, alsa_device_name_.c_str(),
                        SND_PCM_STREAM_PLAYBACK, 0);
    LOG(INFO) << "snd_pcm_open: handle=" << pcm_;
  }

  // Some OEM-developed Cast for Audio devices don't accurately report their
  // support for different output formats, so this tries 32-bit output and then
  // 16-bit output if that fails.
  //
  // TODO(cleichner): Replace this with more specific device introspection.
  // b/24747205
  int err = SetAlsaPlaybackParams();
  if (err < 0) {
    LOG(ERROR) << "Error setting ALSA playback parameters: "
               << alsa_->StrError(err);
    SignalError();
    return;
  }

  // Initialize filters
  for (auto&& filter_group : filter_groups_) {
    filter_group->Initialize(output_samples_per_second_);
  }

  RETURN_REPORT_ERROR(PcmPrepare, pcm_);
  RETURN_REPORT_ERROR(PcmStatusMalloc, &pcm_status_);

  alsa_rendering_delay_.timestamp_microseconds = kNoTimestamp;
  alsa_rendering_delay_.delay_microseconds = 0;

  state_ = kStateNormalPlayback;
}

void StreamMixerAlsa::Stop() {
  for (auto* observer : loopback_observers_) {
    observer->OnLoopbackInterrupted();
  }

  if (alsa_) {
    alsa_->PcmStatusFree(pcm_status_);
    alsa_->PcmHwParamsFree(pcm_hw_params_);
  }

  pcm_status_ = nullptr;
  pcm_hw_params_ = nullptr;
  state_ = kStateUninitialized;
  output_samples_per_second_ = kInvalidSampleRate;

  if (!pcm_) {
    return;
  }

  // If |pcm_| is RUNNING, drain all pending data.
  if (alsa_->PcmState(pcm_) == SND_PCM_STATE_RUNNING) {
    int err = alsa_->PcmDrain(pcm_);
    if (err < 0) {
      LOG(ERROR) << "snd_pcm_drain error: " << alsa_->StrError(err);
    }
  } else {
    int err = alsa_->PcmDrop(pcm_);
    if (err < 0) {
      LOG(ERROR) << "snd_pcm_drop error: " << alsa_->StrError(err);
    }
  }
}

void StreamMixerAlsa::Close() {
  Stop();

  if (!pcm_) {
    return;
  }

  LOG(INFO) << "snd_pcm_close: handle=" << pcm_;
  int err = alsa_->PcmClose(pcm_);
  if (err < 0) {
    LOG(ERROR) << "snd_pcm_close error, leaking handle: "
               << alsa_->StrError(err);
  }
  pcm_ = nullptr;
}

void StreamMixerAlsa::SignalError() {
  state_ = kStateError;
  retry_write_frames_timer_->Stop();
  for (auto&& input : inputs_) {
    input->SignalError(StreamMixerAlsaInput::MixerError::kInternalError);
    ignored_inputs_.push_back(std::move(input));
  }
  inputs_.clear();
  POST_TASK_TO_MIXER_THREAD(&StreamMixerAlsa::Close);
}

void StreamMixerAlsa::SetAlsaWrapperForTest(
    std::unique_ptr<AlsaWrapper> alsa_wrapper) {
  if (alsa_) {
    Close();
  }

  alsa_ = std::move(alsa_wrapper);
}

void StreamMixerAlsa::WriteFramesForTest() {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::WriteFramesForTest);
  WriteFrames();
}

void StreamMixerAlsa::ClearInputsForTest() {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::ClearInputsForTest);
  inputs_.clear();
}

void StreamMixerAlsa::AddInput(std::unique_ptr<InputQueue> input) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::AddInput,
                      base::Passed(std::move(input)));
  if (!alsa_) {
    alsa_.reset(new AlsaWrapper());
  }

  DCHECK(input);

  // If the new input is a primary one, we may need to change the output
  // sample rate to match its input sample rate.
  // We only change the output rate if it is not set to a fixed value.
  if (input->primary() &&
      fixed_output_samples_per_second_ == kInvalidSampleRate) {
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
      Start();
    // Fallthrough intended
    case kStateNormalPlayback: {
      bool found_filter_group = false;
      input->Initialize(alsa_rendering_delay_);
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
      input->SignalError(StreamMixerAlsaInput::MixerError::kInternalError);
      ignored_inputs_.push_back(std::move(input));
      break;
    default:
      NOTREACHED();
  }
}

void StreamMixerAlsa::CheckChangeOutputRate(int input_samples_per_second) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (!pcm_ ||
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
    input->SignalError(StreamMixerAlsaInput::MixerError::kInputIgnored);
    ignored_inputs_.push_back(std::move(input));
  }
  inputs_.clear();

  requested_output_samples_per_second_ = input_samples_per_second;
  // Reset the ALSA params so that the new output sample rate takes effect.
  Stop();
  Start();
}

void StreamMixerAlsa::RemoveInput(InputQueue* input) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::RemoveInput, input);
  DCHECK(input);
  DCHECK(!input->IsDeleting());
  input->PrepareToDelete(
      base::Bind(&StreamMixerAlsa::DeleteInputQueue, base::Unretained(this)));
}

void StreamMixerAlsa::DeleteInputQueue(InputQueue* input) {
  // Always post a task, in case an input calls this while we are iterating
  // through the |inputs_| list.
  POST_TASK_TO_MIXER_THREAD(&StreamMixerAlsa::DeleteInputQueueInternal, input);
}

void StreamMixerAlsa::DeleteInputQueueInternal(InputQueue* input) {
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
          base::Bind(&StreamMixerAlsa::CheckClose, base::Unretained(this)));
    }
  }
}

void StreamMixerAlsa::CheckClose() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(inputs_.empty());
  retry_write_frames_timer_->Stop();
  Close();
}

void StreamMixerAlsa::OnFramesQueued() {
  if (state_ != kStateNormalPlayback) {
    return;
  }

  if (retry_write_frames_timer_->IsRunning()) {
    return;
  }

  retry_write_frames_timer_->Start(
      FROM_HERE, base::TimeDelta(),
      base::Bind(&StreamMixerAlsa::WriteFrames, base::Unretained(this)));
}

void StreamMixerAlsa::WriteFrames() {
  retry_write_frames_timer_->Stop();
  if (TryWriteFrames()) {
    retry_write_frames_timer_->Start(
        FROM_HERE, base::TimeDelta(),
        base::Bind(&StreamMixerAlsa::WriteFrames, base::Unretained(this)));
  }
}

bool StreamMixerAlsa::TryWriteFrames() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK_GE(filter_groups_.size(), 1u);

  if (state_ != kStateNormalPlayback) {
    return false;
  }

  const int min_frames_in_buffer =
      output_samples_per_second_ * kMinBufferedDataMs / 1000;
  int chunk_size = output_samples_per_second_ * kMaxWriteSizeMs / 1000;
  bool is_silence = true;
  for (auto&& filter_group : filter_groups_) {
    filter_group->ClearActiveInputs();
  }
  for (auto&& input : inputs_) {
    int read_size = input->MaxReadSize();
    if (read_size > 0) {
      DCHECK(input->filter_group());
      input->filter_group()->AddActiveInput(input.get());
      chunk_size = std::min(chunk_size, read_size);
      is_silence = false;
    } else if (input->primary()) {
      if (alsa_->PcmStatus(pcm_, pcm_status_) != 0) {
        LOG(ERROR) << "Failed to get status";
        return false;
      }

      int frames_in_buffer =
          alsa_buffer_size_ - alsa_->PcmStatusGetAvail(pcm_status_);
      if (alsa_->PcmStatusGetState(pcm_status_) == SND_PCM_STATE_XRUN ||
          frames_in_buffer < min_frames_in_buffer) {
        // If there has been (or soon will be) an underrun, continue without the
        // empty primary input stream.
        input->OnSkipped();
        continue;
      }

      // A primary input cannot provide any data, so wait until later.
      retry_write_frames_timer_->Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(kMinBufferedDataMs / 2),
          base::Bind(&StreamMixerAlsa::WriteFrames, base::Unretained(this)));
      return false;
    } else {
      input->OnSkipped();
    }
  }

  if (is_silence) {
    // No inputs have any data to provide. Push silence to prevent underrun.
    chunk_size = kPreventUnderrunChunkSize;
  }

  // Recursively mix and filter each group.
  linearize_filter_->MixAndFilter(chunk_size);

  WriteMixedPcm(chunk_size);
  return true;
}

size_t StreamMixerAlsa::InterleavedSize(int frames) {
  return BytesPerOutputFormatSample() *
         static_cast<size_t>(frames * num_output_channels_);
}

ssize_t StreamMixerAlsa::BytesPerOutputFormatSample() {
  return alsa_->PcmFormatSize(pcm_format_, 1);
}

void StreamMixerAlsa::WriteMixedPcm(int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  CHECK_PCM_INITIALIZED();

  // Resize interleaved if necessary.
  size_t interleaved_size = static_cast<size_t>(frames) * num_output_channels_ *
                            BytesPerOutputFormatSample();

  int64_t expected_playback_time;
  if (alsa_rendering_delay_.timestamp_microseconds == kNoTimestamp) {
    expected_playback_time = kNoTimestamp;
  } else {
    expected_playback_time = alsa_rendering_delay_.timestamp_microseconds +
                             alsa_rendering_delay_.delay_microseconds +
                             linearize_filter_->GetRenderingDelayMicroseconds();
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
        InterleavedSize(frames));
  }

  uint8_t* data;
  if (pcm_format_ == SND_PCM_FORMAT_FLOAT) {
    // Hard limit to [1.0, -1.0]. ToFixedPoint handles this for other cases.
    for (int i = 0; i < frames * num_output_channels_; ++i) {
      linearize_filter_->interleaved()[i] =
          std::min(1.0f, std::max(-1.0f, linearize_filter_->interleaved()[i]));
    }
    data = reinterpret_cast<uint8_t*>(linearize_filter_->interleaved());
  } else {
    if (interleaved_.size() < interleaved_size) {
      interleaved_.resize(interleaved_size);
    }
    ToFixedPoint(linearize_filter_->interleaved(),
                 frames * num_output_channels_, BytesPerOutputFormatSample(),
                 interleaved_.data());
    data = interleaved_.data();
  }

  // If the PCM has been drained it will be in SND_PCM_STATE_SETUP and need
  // to be prepared in order for playback to work.
  if (alsa_->PcmState(pcm_) == SND_PCM_STATE_SETUP) {
    RETURN_REPORT_ERROR(PcmPrepare, pcm_);
  }

  int frames_left = frames;
  while (frames_left) {
    int frames_or_error;
    while ((frames_or_error = alsa_->PcmWritei(pcm_, data, frames_left)) < 0) {
      for (auto* observer : loopback_observers_) {
        observer->OnLoopbackInterrupted();
      }
      RETURN_REPORT_ERROR(PcmRecover, pcm_, frames_or_error,
                          kPcmRecoverIsSilent);
    }
    frames_left -= frames_or_error;
    DCHECK_GE(frames_left, 0);
    data +=
        frames_or_error * num_output_channels_ * BytesPerOutputFormatSample();
  }
  UpdateRenderingDelay(frames);
  MediaPipelineBackendAlsa::RenderingDelay common_rendering_delay =
      alsa_rendering_delay_;
  common_rendering_delay.delay_microseconds +=
      linearize_filter_->GetRenderingDelayMicroseconds() +
      mix_filter_->GetRenderingDelayMicroseconds();
  for (auto&& input : inputs_) {
    MediaPipelineBackendAlsa::RenderingDelay stream_rendering_delay =
        common_rendering_delay;
    stream_rendering_delay.delay_microseconds +=
        input->filter_group()->GetRenderingDelayMicroseconds();
    input->AfterWriteFrames(stream_rendering_delay);
  }
}

void StreamMixerAlsa::UpdateRenderingDelay(int newly_pushed_frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  CHECK_PCM_INITIALIZED();

  // TODO(bshaya): Add rendering delay from post-processors.
  if (alsa_->PcmStatus(pcm_, pcm_status_) != 0 ||
      alsa_->PcmStatusGetState(pcm_status_) != SND_PCM_STATE_RUNNING) {
    alsa_rendering_delay_.timestamp_microseconds = kNoTimestamp;
    alsa_rendering_delay_.delay_microseconds = 0;
    return;
  }

  snd_htimestamp_t status_timestamp = {};
  alsa_->PcmStatusGetHtstamp(pcm_status_, &status_timestamp);
  alsa_rendering_delay_.timestamp_microseconds =
      TimespecToMicroseconds(status_timestamp);
  snd_pcm_sframes_t delay_frames = alsa_->PcmStatusGetDelay(pcm_status_);
  alsa_rendering_delay_.delay_microseconds =
      static_cast<int64_t>(delay_frames) * base::Time::kMicrosecondsPerSecond /
      output_samples_per_second_;
}

void StreamMixerAlsa::AddLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::AddLoopbackAudioObserver, observer);
  DCHECK(observer);
  DCHECK(!base::ContainsValue(loopback_observers_, observer));
  loopback_observers_.push_back(observer);
}

void StreamMixerAlsa::RemoveLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::RemoveLoopbackAudioObserver, observer);
  DCHECK(base::ContainsValue(loopback_observers_, observer));
  loopback_observers_.erase(std::remove(loopback_observers_.begin(),
                                        loopback_observers_.end(), observer),
                            loopback_observers_.end());
  observer->OnRemoved();
}

void StreamMixerAlsa::SetVolume(AudioContentType type, float level) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::SetVolume, type, level);
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

void StreamMixerAlsa::SetMuted(AudioContentType type, bool muted) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::SetMuted, type, muted);
  volume_info_[type].muted = muted;
  for (auto&& input : inputs_) {
    if (input->content_type() == type) {
      input->SetMuted(muted);
    }
  }
}

void StreamMixerAlsa::SetOutputLimit(AudioContentType type, float limit) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::SetOutputLimit, type, limit);
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

void StreamMixerAlsa::SetPostProcessorConfig(const std::string& name,
                                             const std::string& message) {
  RUN_ON_MIXER_THREAD(&StreamMixerAlsa::SetPostProcessorConfig, name, message);
  for (auto&& filter_group : filter_groups_) {
    filter_group->SetPostProcessorConfig(name, message);
  }
}

}  // namespace media
}  // namespace chromecast
