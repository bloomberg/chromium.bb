// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/common/mojom/constants.mojom.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/cma/backend/cma_backend.h"
#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"

namespace {
const int kMaxQueuedDataMs = 1000;
}  // namespace

namespace chromecast {
namespace media {

// Backend represents a CmaBackend adapter.
// It can be created and destroyed on any thread,
// but all other member functions must be called on a single thread.
class CastAudioOutputStream::Backend : public CmaBackend::Decoder::Delegate {
 public:
  using BindConnectorRequestCallback = base::OnceCallback<void(
      service_manager::mojom::ConnectorRequest request)>;

  explicit Backend(const ::media::AudioParameters& audio_params)
      : audio_params_(audio_params),
        timestamp_helper_(audio_params_.sample_rate()),
        buffer_duration_(audio_params.GetBufferDuration()),
        first_start_(true),
        push_in_progress_(false),
        encountered_error_(false),
        decoder_(nullptr),
        source_callback_(nullptr),
        weak_factory_(this) {
    DETACH_FROM_THREAD(thread_checker_);
  }
  ~Backend() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (backend_ && !first_start_)  // Only stop the backend if it was started.
      backend_->Stop();
  }

  void Open(CastAudioManager* audio_manager,
            BindConnectorRequestCallback bind_connector_request_cb) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(audio_manager);
    DCHECK(backend_ == nullptr);

    // TODO(awolter, b/111669896): Populate this with the correct session id.
    const std::string application_session_id = "";

    // Create a connector bound to the |browser_task_runner_| in CAOS.
    service_manager::mojom::ConnectorRequest connector_request;
    auto connector = service_manager::Connector::Create(&connector_request);
    std::move(bind_connector_request_cb).Run(std::move(connector_request));

    // Connect to the Multiroom interface and fetch the current info.
    connector->BindInterface(chromecast::mojom::kChromecastServiceName,
                             &multiroom_manager_);
    multiroom_manager_.set_connection_error_handler(base::BindOnce(
        &CastAudioOutputStream::Backend::OnGetMultiroomInfo,
        base::Unretained(this), audio_manager, application_session_id,
        chromecast::mojom::MultiroomInfo::New()));
    multiroom_manager_->GetMultiroomInfo(
        application_session_id,
        base::BindOnce(&CastAudioOutputStream::Backend::OnGetMultiroomInfo,
                       base::Unretained(this), audio_manager,
                       application_session_id));
  }

  void OnGetMultiroomInfo(CastAudioManager* audio_manager,
                          const std::string& application_session_id,
                          chromecast::mojom::MultiroomInfoPtr multiroom_info) {
    audio_manager->backend_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CastAudioOutputStream::Backend::ProcessMultiroomInfo,
                       base::Unretained(this), audio_manager,
                       application_session_id, std::move(multiroom_info)));
  }

  void ProcessMultiroomInfo(
      CastAudioManager* audio_manager,
      const std::string& application_session_id,
      chromecast::mojom::MultiroomInfoPtr multiroom_info) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(multiroom_info);
    CMALOG(kLogControl) << __FUNCTION__ << ": " << this
                        << " session_id=" << application_session_id
                        << ", multiroom=" << multiroom_info->multiroom
                        << ", audio_channel=" << multiroom_info->audio_channel;

    // Close the MultiroomManager message pipe so that a connection error does
    // not trigger a second call to this function.
    multiroom_manager_.reset();

    CmaBackendFactory* backend_factory = audio_manager->backend_factory();
    DCHECK(backend_factory);

    MediaPipelineDeviceParams::AudioStreamType stream_type =
        MediaPipelineDeviceParams::kAudioStreamSoundEffects;
    if (audio_params_.effects() & ::media::AudioParameters::MULTIZONE) {
      stream_type = MediaPipelineDeviceParams::kAudioStreamNormal;
    }

    backend_task_runner_.reset(new TaskRunnerImpl());
    MediaPipelineDeviceParams device_params(
        MediaPipelineDeviceParams::kModeIgnorePts, stream_type,
        backend_task_runner_.get(), AudioContentType::kMedia,
        ::media::AudioDeviceDescription::kDefaultDeviceId);
    device_params.session_id = application_session_id;
    device_params.multiroom = multiroom_info->multiroom;
    device_params.audio_channel = multiroom_info->audio_channel;
    device_params.output_delay_us =
        multiroom_info->output_delay.InMicroseconds();
    backend_ = backend_factory->CreateBackend(device_params);
    if (!backend_) {
      encountered_error_ = true;
      return;
    }

    decoder_ = backend_->CreateAudioDecoder();
    if (!decoder_) {
      encountered_error_ = true;
      return;
    }
    decoder_->SetDelegate(this);

    AudioConfig audio_config;
    audio_config.codec = kCodecPCM;
    audio_config.sample_format = kSampleFormatS16;
    audio_config.bytes_per_channel = 2;
    audio_config.channel_number = audio_params_.channels();
    audio_config.samples_per_second = audio_params_.sample_rate();
    if (!decoder_->SetConfig(audio_config)) {
      encountered_error_ = true;
      return;
    }

    if (!backend_->Initialize()) {
      encountered_error_ = true;
      return;
    }

    audio_bus_ = ::media::AudioBus::Create(audio_params_);
    decoder_buffer_ = new DecoderBufferAdapter(new ::media::DecoderBuffer(
        audio_params_.GetBytesPerBuffer(::media::kSampleFormatS16)));
    timestamp_helper_.SetBaseTimestamp(base::TimeDelta());

    if (pending_start_) {
      std::move(pending_start_).Run();
    }
    if (pending_volume_) {
      std::move(pending_volume_).Run();
    }
  }

  void Start(AudioSourceCallback* source_callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!backend_) {
      pending_start_ = base::BindOnce(&CastAudioOutputStream::Backend::Start,
                                      base::Unretained(this), source_callback);
      return;
    }

    source_callback_ = source_callback;
    if (encountered_error_) {
      source_callback_->OnError();
      return;
    }

    if (first_start_) {
      first_start_ = false;
      backend_->Start(0);
    } else {
      backend_->Resume();
    }

    next_push_time_ = base::TimeTicks::Now();
    if (!push_in_progress_) {
      push_in_progress_ = true;
      PushBuffer();
    }
  }

  void Stop(base::OnceClosure completion_cb) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    pending_start_.Reset();
    pending_volume_.Reset();

    if (backend_) {
      DCHECK(backend_);
      backend_->Pause();
    }

    source_callback_ = nullptr;
    std::move(completion_cb).Run();
  }

  void SetVolume(double volume) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!decoder_) {
      pending_volume_ =
          base::BindOnce(&CastAudioOutputStream::Backend::SetVolume,
                         base::Unretained(this), volume);
      return;
    }

    if (encountered_error_) {
      return;
    }

    decoder_->SetVolume(volume);
  }

 private:
  void PushBuffer() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(push_in_progress_);

    if (!source_callback_ || encountered_error_) {
      push_in_progress_ = false;
      return;
    }

    CmaBackend::AudioDecoder::RenderingDelay rendering_delay =
        decoder_->GetRenderingDelay();
    base::TimeDelta delay =
        base::TimeDelta::FromMicroseconds(rendering_delay.delay_microseconds);
    base::TimeTicks delay_timestamp =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
                                rendering_delay.timestamp_microseconds);
    int frame_count = source_callback_->OnMoreData(delay, delay_timestamp, 0,
                                                   audio_bus_.get());
    VLOG(3) << "frames_filled=" << frame_count << " with latency=" << delay;

    DCHECK_EQ(frame_count, audio_bus_->frames());
    DCHECK_EQ(static_cast<int>(decoder_buffer_->data_size()),
              audio_params_.GetBytesPerBuffer(::media::kSampleFormatS16));
    audio_bus_->ToInterleaved<::media::SignedInt16SampleTypeTraits>(
        frame_count,
        reinterpret_cast<int16_t*>(decoder_buffer_->writable_data()));
    decoder_buffer_->set_timestamp(timestamp_helper_.GetTimestamp());
    timestamp_helper_.AddFrames(frame_count);

    BufferStatus status = decoder_->PushBuffer(decoder_buffer_.get());
    if (status != CmaBackend::BufferStatus::kBufferPending)
      OnPushBufferComplete(status);
  }

  // CmaBackend::Decoder::Delegate implementation
  void OnPushBufferComplete(BufferStatus status) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_NE(status, CmaBackend::BufferStatus::kBufferPending);

    DCHECK(push_in_progress_);
    push_in_progress_ = false;

    if (!source_callback_ || encountered_error_)
      return;

    if (status != CmaBackend::BufferStatus::kBufferSuccess) {
      source_callback_->OnError();
      return;
    }

    // Schedule next push buffer. We don't want to allow more than
    // kMaxQueuedDataMs of queued audio.
    const base::TimeTicks now = base::TimeTicks::Now();
    next_push_time_ = std::max(now, next_push_time_ + buffer_duration_);

    base::TimeDelta delay = (next_push_time_ - now) -
                            base::TimeDelta::FromMilliseconds(kMaxQueuedDataMs);
    delay = std::max(delay, base::TimeDelta());

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Backend::PushBuffer, weak_factory_.GetWeakPtr()),
        delay);
    push_in_progress_ = true;
  }

  void OnEndOfStream() override {}

  void OnDecoderError() override {
    VLOG(1) << this << ": " << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    encountered_error_ = true;
    if (source_callback_)
      source_callback_->OnError();
  }

  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override {}

  void OnVideoResolutionChanged(const Size& size) override {}

  const ::media::AudioParameters audio_params_;
  std::unique_ptr<::media::AudioBus> audio_bus_;
  scoped_refptr<media::DecoderBufferBase> decoder_buffer_;
  ::media::AudioTimestampHelper timestamp_helper_;
  const base::TimeDelta buffer_duration_;
  bool first_start_;
  bool push_in_progress_;
  bool encountered_error_;
  base::TimeTicks next_push_time_;
  std::unique_ptr<TaskRunnerImpl> backend_task_runner_;
  std::unique_ptr<CmaBackend> backend_;
  CmaBackend::AudioDecoder* decoder_;
  AudioSourceCallback* source_callback_;
  chromecast::mojom::MultiroomManagerPtr multiroom_manager_;

  // Hold bindings to Start and SetVolume if they were called before Open
  // completed. After initialization has finished, these bindings will be
  // called.
  base::OnceCallback<void()> pending_start_;
  base::OnceCallback<void()> pending_volume_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<Backend> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Backend);
};

// CastAudioOutputStream runs on audio thread (AudioManager::GetTaskRunner).
CastAudioOutputStream::CastAudioOutputStream(
    const ::media::AudioParameters& audio_params,
    scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
    service_manager::Connector* browser_connector,
    CastAudioManager* audio_manager)
    : audio_params_(audio_params),
      browser_task_runner_(browser_task_runner),
      browser_connector_(browser_connector),
      audio_manager_(audio_manager),
      volume_(1.0) {
  DCHECK(browser_task_runner_);
  DCHECK(browser_connector_);
  VLOG(1) << "CastAudioOutputStream " << this << " created with "
          << audio_params_.AsHumanReadableString();
}

CastAudioOutputStream::~CastAudioOutputStream() {
  DCHECK(!backend_);
}

bool CastAudioOutputStream::Open() {
  VLOG(1) << this << ": " << __func__;
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  ::media::AudioParameters::Format format = audio_params_.format();
  DCHECK((format == ::media::AudioParameters::AUDIO_PCM_LINEAR) ||
         (format == ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY));

  ::media::ChannelLayout channel_layout = audio_params_.channel_layout();
  if ((channel_layout != ::media::CHANNEL_LAYOUT_MONO) &&
      (channel_layout != ::media::CHANNEL_LAYOUT_STEREO)) {
    LOG(WARNING) << "Unsupported channel layout: " << channel_layout;
    return false;
  }
  DCHECK_GE(audio_params_.channels(), 1);
  DCHECK_LE(audio_params_.channels(), 2);

  DCHECK(!backend_);
  backend_ = std::make_unique<Backend>(audio_params_);
  audio_manager_->backend_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &Backend::Open, base::Unretained(backend_.get()), audio_manager_,
          base::BindOnce(&CastAudioOutputStream::BindConnectorRequest,
                         base::Unretained(this))));

  // Always return success even though we are unsure at this point if the
  // backend has opened successfully. Errors will be reported via the
  // AudioSourceCallback after Start() has been issued.
  //
  // Failing early is convenient for falling back to other audio stream types,
  // but Cast does not need the fallback, so it is alright to fail late with a
  // callback.
  return true;
}

void CastAudioOutputStream::Close() {
  VLOG(1) << this << ": " << __func__;
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  DCHECK(backend_);
  audio_manager_->backend_task_runner()->DeleteSoon(FROM_HERE,
                                                    backend_.release());

  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  audio_manager_->ReleaseOutputStream(this);
}

void CastAudioOutputStream::Start(AudioSourceCallback* source_callback) {
  VLOG(2) << this << ": " << __func__;
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(source_callback);
  DCHECK(backend_);

  audio_manager_->backend_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::Start, base::Unretained(backend_.get()),
                     source_callback));

  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstAudio();
}

void CastAudioOutputStream::Stop() {
  VLOG(2) << this << ": " << __func__;
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(backend_);

  base::WaitableEvent completion_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  audio_manager_->backend_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::Stop, base::Unretained(backend_.get()),
                     base::BindOnce(&base::WaitableEvent::Signal,
                                    base::Unretained(&completion_event))));
  completion_event.Wait();
}

void CastAudioOutputStream::SetVolume(double volume) {
  VLOG(2) << this << ": " << __func__ << "(" << volume << ")";
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  volume_ = volume;
  if (backend_) {
    audio_manager_->backend_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Backend::SetVolume,
                                  base::Unretained(backend_.get()), volume_));
  }
}

void CastAudioOutputStream::GetVolume(double* volume) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  *volume = volume_;
}

void CastAudioOutputStream::BindConnectorRequest(
    service_manager::mojom::ConnectorRequest connector_request) {
  // Ensure that we bind the connector request on the browser thread because we
  // use the browser thread's connector for the bind.
  browser_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CastAudioOutputStream::BindConnectorRequestOnBrowserTaskRunner,
          base::Unretained(this), std::move(connector_request)));
}

void CastAudioOutputStream::BindConnectorRequestOnBrowserTaskRunner(
    service_manager::mojom::ConnectorRequest connector_request) {
  browser_connector_->BindConnectorRequest(std::move(connector_request));
}

}  // namespace media
}  // namespace chromecast
