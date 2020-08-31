// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/mirroring/service/captured_audio_input.h"
#include "components/mirroring/service/udp_socket_client.h"
#include "components/mirroring/service/video_capture_client.h"
#include "crypto/random.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/audio/audio_input_device.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/bind_to_current_loop.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/ip_endpoint.h"
#include "services/viz/public/cpp/gpu/gpu.h"

using media::cast::CastTransportStatus;
using media::cast::Codec;
using media::cast::FrameEvent;
using media::cast::FrameSenderConfig;
using media::cast::OperationalStatus;
using media::cast::Packet;
using media::cast::PacketEvent;
using media::cast::RtpPayloadType;
using media::mojom::RemotingSinkAudioCapability;
using media::mojom::RemotingSinkVideoCapability;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionType;

namespace mirroring {

namespace {

// The interval for CastTransport to send Frame/PacketEvents to Session for
// logging.
constexpr base::TimeDelta kSendEventsInterval = base::TimeDelta::FromSeconds(1);

// The duration for OFFER/ANSWER exchange. If timeout, notify the client that
// the session failed to start.
constexpr base::TimeDelta kOfferAnswerExchangeTimeout =
    base::TimeDelta::FromSeconds(15);

// Amount of time to wait before assuming the Cast Receiver does not support
// querying for capabilities via GET_CAPABILITIES.
constexpr base::TimeDelta kGetCapabilitiesTimeout =
    base::TimeDelta::FromSeconds(30);

// Used for OFFER/ANSWER message exchange. Some receivers will error out on
// payloadType values other than the ones hard-coded here.
constexpr int kAudioPayloadType = 127;
constexpr int kVideoPayloadType = 96;

constexpr int kAudioSsrcMin = 1;
constexpr int kAudioSsrcMax = 5e5;
constexpr int kVideoSsrcMin = 5e5 + 1;
constexpr int kVideoSsrcMax = 10e5;

// Maximum number of bytes of file data allowed in a single Crash report. As of
// this writing, the total report upload size is capped at 20 MB.
//
// 2 KB of "overhead bytes" are subtracted to account for all of the non-file
// data in a report upload, including HTTP headers/requests and form data.
constexpr int kMaxCrashReportBytes = (20 * 1024 - 2) * 1024;

class TransportClient final : public media::cast::CastTransport::Client {
 public:
  explicit TransportClient(Session* session) : session_(session) {}
  ~TransportClient() override {}

  // media::cast::CastTransport::Client implementation.

  void OnStatusChanged(CastTransportStatus status) override {
    session_->OnTransportStatusChanged(status);
  }

  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) override {
    session_->OnLoggingEventsReceived(std::move(frame_events),
                                      std::move(packet_events));
  }

  void ProcessRtpPacket(std::unique_ptr<Packet> packet) override {
    NOTREACHED();
  }

 private:
  Session* const session_;  // Outlives this class.

  DISALLOW_COPY_AND_ASSIGN(TransportClient);
};

// Generates a string with cryptographically secure random bytes.
std::string MakeRandomString(size_t length) {
  std::string result(length, ' ');
  crypto::RandBytes(base::data(result), length);
  return result;
}

int NumberOfEncodeThreads() {
  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  return std::min(8, (base::SysInfo::NumberOfProcessors() + 1) / 2);
}

// Scan profiles for hardware VP8 encoder support.
bool IsHardwareVP8EncodingSupported(
    const std::vector<media::VideoEncodeAccelerator::SupportedProfile>&
        profiles) {
  for (const auto& vea_profile : profiles) {
    if (vea_profile.profile >= media::VP8PROFILE_MIN &&
        vea_profile.profile <= media::VP8PROFILE_MAX) {
      return true;
    }
  }
  return false;
}

// Scan profiles for hardware H.264 encoder support.
bool IsHardwareH264EncodingSupported(
    const std::vector<media::VideoEncodeAccelerator::SupportedProfile>&
        profiles) {
// TODO(crbug.com/1015482): Look into why H.264 hardware encoder on MacOS is
// broken.
// TODO(crbug.com/1015482): Look into HW encoder initialization issues on Win.
#if !defined(OS_MACOSX) && !defined(OS_WIN)
  for (const auto& vea_profile : profiles) {
    if (vea_profile.profile >= media::H264PROFILE_MIN &&
        vea_profile.profile <= media::H264PROFILE_MAX) {
      return true;
    }
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_WIN)
  return false;
}

// Helper to add |config| to |config_list| with given |aes_key|.
void AddSenderConfig(int32_t sender_ssrc,
                     FrameSenderConfig config,
                     const std::string& aes_key,
                     const std::string& aes_iv,
                     const mojom::SessionParameters& session_params,
                     std::vector<FrameSenderConfig>* config_list) {
  config.aes_key = aes_key;
  config.aes_iv_mask = aes_iv;
  config.sender_ssrc = sender_ssrc;
  if (session_params.target_playout_delay) {
    config.animated_playout_delay = session_params.target_playout_delay.value();
    config.min_playout_delay = session_params.target_playout_delay.value();
    config.max_playout_delay = session_params.target_playout_delay.value();
  }
  config_list->emplace_back(config);
}

// Generate the stream object from |config| and add it to |stream_list|.
void AddStreamObject(int stream_index,
                     const std::string& codec_name,
                     const FrameSenderConfig& config,
                     const MirrorSettings& mirror_settings,
                     base::Value::ListStorage* stream_list) {
  base::Value stream(base::Value::Type::DICTIONARY);
  stream.SetKey("index", base::Value(stream_index));
  stream.SetKey("codecName", base::Value(base::ToLowerASCII(codec_name)));
  stream.SetKey("rtpProfile", base::Value("cast"));
  const bool is_audio =
      (config.rtp_payload_type <= media::cast::RtpPayloadType::AUDIO_LAST);
  stream.SetKey("rtpPayloadType",
                base::Value(is_audio ? kAudioPayloadType : kVideoPayloadType));
  stream.SetKey("ssrc", base::Value(static_cast<int>(config.sender_ssrc)));
  stream.SetKey("targetDelay",
                base::Value(static_cast<int>(
                    config.animated_playout_delay.InMilliseconds())));
  stream.SetKey("aesKey", base::Value(base::HexEncode(config.aes_key.data(),
                                                      config.aes_key.size())));
  stream.SetKey("aesIvMask",
                base::Value(base::HexEncode(config.aes_iv_mask.data(),
                                            config.aes_iv_mask.size())));
  stream.SetKey("timeBase",
                base::Value("1/" + std::to_string(config.rtp_timebase)));
  stream.SetKey("receiverRtcpEventLog", base::Value(true));
  stream.SetKey("rtpExtensions", base::Value("adaptive_playout_delay"));
  if (is_audio) {
    // Note on "AUTO" bitrate calculation: This is based on libopus source
    // at the time of this writing. Internally, it uses the following math:
    //
    //   packet_overhead_bps = 60 bits * num_packets_in_one_second
    //   approx_encoded_signal_bps = frequency * channels
    //   estimated_bps = packet_overhead_bps + approx_encoded_signal_bps
    //
    // For 100 packets/sec at 48 kHz and 2 channels, this is 102kbps.
    const int bitrate = config.max_bitrate > 0
                            ? config.max_bitrate
                            : (60 * config.max_frame_rate +
                               config.rtp_timebase * config.channels);
    stream.SetKey("type", base::Value("audio_source"));
    stream.SetKey("bitRate", base::Value(bitrate));
    stream.SetKey("sampleRate", base::Value(config.rtp_timebase));
    stream.SetKey("channels", base::Value(config.channels));
  } else /* is video */ {
    stream.SetKey("type", base::Value("video_source"));
    stream.SetKey("renderMode", base::Value("video"));
    stream.SetKey("maxFrameRate",
                  base::Value(std::to_string(static_cast<int>(
                                  config.max_frame_rate * 1000)) +
                              "/1000"));
    stream.SetKey("maxBitRate", base::Value(config.max_bitrate));
    base::Value::ListStorage resolutions;
    base::Value resolution(base::Value::Type::DICTIONARY);
    resolution.SetKey("width", base::Value(mirror_settings.max_width()));
    resolution.SetKey("height", base::Value(mirror_settings.max_height()));
    resolutions.emplace_back(std::move(resolution));
    stream.SetKey("resolutions", base::Value(resolutions));
  }
  stream_list->emplace_back(std::move(stream));
}

// Checks whether receiver's build version is less than "1.|base_version|.xxxx".
// Returns true if given version doesn't have the format of "1.xx.xxxx", so that
// we don't assume that the receiver has the required new capabilities.
bool NeedsWorkaroundForOlder1DotXVersions(
    const std::string& receiver_build_version,
    int base_version) {
  if (!base::StartsWith(receiver_build_version, "1.",
                        base::CompareCase::SENSITIVE)) {
    return true;
  }
  const size_t end_pos = receiver_build_version.find_first_of('.', 2);
  if (end_pos == std::string::npos)
    return false;
  int version = 0;
  return (base::StringToInt(receiver_build_version.substr(2, end_pos - 2),
                            &version) &&
          version < base_version);
}

// Convert the sink capabilities to media::mojom::RemotingSinkMetadata.
media::mojom::RemotingSinkMetadata ToRemotingSinkMetadata(
    const std::vector<std::string>& capabilities,
    const std::string& receiver_name,
    const mojom::SessionParameters& params,
    const std::string& receiver_build_version) {
  media::mojom::RemotingSinkMetadata sink_metadata;
  sink_metadata.friendly_name = receiver_name;

  for (const auto& capability : capabilities) {
    if (capability == "audio") {
      sink_metadata.audio_capabilities.push_back(
          RemotingSinkAudioCapability::CODEC_BASELINE_SET);
    } else if (capability == "aac") {
      sink_metadata.audio_capabilities.push_back(
          RemotingSinkAudioCapability::CODEC_AAC);
    } else if (capability == "opus") {
      sink_metadata.audio_capabilities.push_back(
          RemotingSinkAudioCapability::CODEC_OPUS);
    } else if (capability == "video") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::CODEC_BASELINE_SET);
    } else if (capability == "4k") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::SUPPORT_4K);
    } else if (capability == "h264") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::CODEC_H264);
    } else if (capability == "vp8") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::CODEC_VP8);
    } else if (capability == "vp9") {
      // Before 1.27 Earth receivers report "vp9" even though they don't support
      // remoting the VP9 encoded video.
      if (!NeedsWorkaroundForOlder1DotXVersions(receiver_build_version, 27) ||
          base::StartsWith(params.receiver_model_name, "Chromecast Ultra",
                           base::CompareCase::SENSITIVE)) {
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_VP9);
      }
    } else if (capability == "hevc") {
      // Before 1.27 Earth receivers report "hevc" even though they don't
      // support remoting the HEVC encoded video.
      if (!NeedsWorkaroundForOlder1DotXVersions(receiver_build_version, 27) ||
          base::StartsWith(params.receiver_model_name, "Chromecast Ultra",
                           base::CompareCase::SENSITIVE)) {
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_HEVC);
      }
    } else {
      DVLOG(1) << "Unknown mediaCap name: " << capability;
    }
  }

  // Enable remoting 1080p 30fps or higher resolution/fps content for Chromecast
  // Ultra receivers only.
  // TODO(crbug.com/1015467): Receiver should report this capability.
  if (params.receiver_model_name == "Chromecast Ultra") {
    sink_metadata.video_capabilities.push_back(
        RemotingSinkVideoCapability::SUPPORT_4K);
  }

  return sink_metadata;
}

}  // namespace

class Session::AudioCapturingCallback final
    : public media::AudioCapturerSource::CaptureCallback {
 public:
  using AudioDataCallback =
      base::RepeatingCallback<void(std::unique_ptr<media::AudioBus> audio_bus,
                                   const base::TimeTicks& recorded_time)>;
  AudioCapturingCallback(AudioDataCallback audio_data_callback,
                         base::OnceClosure error_callback)
      : audio_data_callback_(std::move(audio_data_callback)),
        error_callback_(std::move(error_callback)) {
    DCHECK(!audio_data_callback_.is_null());
  }

  ~AudioCapturingCallback() override {}

 private:
  // media::AudioCapturerSource::CaptureCallback implementation.
  void OnCaptureStarted() override {}

  // Called on audio thread.
  void Capture(const media::AudioBus* audio_bus,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override {
    // TODO(crbug.com/1015467): Don't copy the audio data. Instead, send
    // |audio_bus| directly to the encoder.
    std::unique_ptr<media::AudioBus> captured_audio =
        media::AudioBus::Create(audio_bus->channels(), audio_bus->frames());
    audio_bus->CopyTo(captured_audio.get());
    audio_data_callback_.Run(std::move(captured_audio), audio_capture_time);
  }

  void OnCaptureError(const std::string& message) override {
    if (!error_callback_.is_null())
      std::move(error_callback_).Run();
  }

  void OnCaptureMuted(bool is_muted) override {}

  const AudioDataCallback audio_data_callback_;
  base::OnceClosure error_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioCapturingCallback);
};

Session::Session(
    mojom::SessionParametersPtr session_params,
    const gfx::Size& max_resolution,
    mojo::PendingRemote<mojom::SessionObserver> observer,
    mojo::PendingRemote<mojom::ResourceProvider> resource_provider,
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : session_params_(*session_params),
      state_(MIRRORING),
      observer_(std::move(observer)),
      resource_provider_(std::move(resource_provider)),
      message_dispatcher_(std::move(outbound_channel),
                          std::move(inbound_channel),
                          base::BindRepeating(&Session::OnResponseParsingError,
                                              base::Unretained(this))),
      gpu_channel_host_(nullptr) {
  DCHECK(resource_provider_);
  mirror_settings_.SetResolutionContraints(max_resolution.width(),
                                           max_resolution.height());
  resource_provider_->GetNetworkContext(
      network_context_.BindNewPipeAndPassReceiver());

  if (session_params->type != mojom::SessionType::AUDIO_ONLY &&
      io_task_runner) {
    mojo::PendingRemote<viz::mojom::Gpu> remote_gpu;
    resource_provider_->BindGpu(remote_gpu.InitWithNewPipeAndPassReceiver());
    gpu_ = viz::Gpu::Create(std::move(remote_gpu), io_task_runner);
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  network_context_->CreateURLLoaderFactory(
      url_loader_factory.InitWithNewPipeAndPassReceiver(), std::move(params));

  // Generate session level tags.
  base::Value session_tags(base::Value::Type::DICTIONARY);
  session_tags.SetKey("mirrorSettings", mirror_settings_.ToDictionaryValue());
  session_tags.SetKey(
      "shouldCaptureAudio",
      base::Value(session_params_.type != SessionType::VIDEO_ONLY));
  session_tags.SetKey(
      "shouldCaptureVideo",
      base::Value(session_params_.type != SessionType::AUDIO_ONLY));
  session_tags.SetKey("receiverProductName",
                      base::Value(session_params_.receiver_model_name));

  session_monitor_.emplace(
      kMaxCrashReportBytes, session_params_.receiver_address,
      std::move(session_tags), std::move(url_loader_factory));

  if (gpu_) {
    gpu_channel_host_ = gpu_->EstablishGpuChannelSync();
    if (gpu_channel_host_ &&
        gpu_channel_host_->gpu_feature_info().status_values
                [gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] ==
            gpu::kGpuFeatureStatusEnabled) {
      supported_profiles_ =
          media::GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
              gpu_channel_host_->gpu_info()
                  .video_encode_accelerator_supported_profiles);
    }
  }

  if (supported_profiles_.empty()) {
    // HW encoding is not supported.
    gpu_channel_host_ = nullptr;
    gpu_.reset();
  }
  CreateAndSendOffer();
}

Session::~Session() {
  StopSession();
}

void Session::ReportError(SessionError error) {
  UMA_HISTOGRAM_ENUMERATION("MediaRouter.MirroringService.SessionError", error);
  if (session_monitor_.has_value())
    session_monitor_->OnStreamingError(error);
  if (state_ == REMOTING) {
    media_remoter_->OnRemotingFailed();  // Try to fallback on mirroring.
    return;
  }

  // Report the error and stop this session.
  if (observer_)
    observer_->OnError(error);
  StopSession();
}

void Session::StopStreaming() {
  DVLOG(2) << __func__ << " state=" << state_;
  if (!cast_environment_)
    return;

  session_monitor_->StopStreamingSession();
  if (audio_input_device_) {
    audio_input_device_->Stop();
    audio_input_device_ = nullptr;
  }
  audio_capturing_callback_.reset();
  audio_stream_.reset();
  video_stream_.reset();
  cast_transport_.reset();
  cast_environment_ = nullptr;
}

void Session::StopSession() {
  DVLOG(1) << __func__;
  if (state_ == STOPPED)
    return;

  state_ = STOPPED;
  StopStreaming();

  session_monitor_.reset();
  weak_factory_.InvalidateWeakPtrs();
  audio_encode_thread_ = nullptr;
  video_encode_thread_ = nullptr;
  video_capture_client_.reset();
  media_remoter_.reset();
  resource_provider_.reset();
  gpu_channel_host_ = nullptr;
  gpu_.reset();
  if (observer_) {
    observer_->DidStop();
    observer_.reset();
  }
}

void Session::OnError(const std::string& message) {
  ReportError(SessionError::RTP_STREAM_ERROR);
}

void Session::RequestRefreshFrame() {
  DVLOG(3) << __func__;
  if (video_capture_client_)
    video_capture_client_->RequestRefreshFrame();
}

void Session::OnEncoderStatusChange(OperationalStatus status) {
  switch (status) {
    case OperationalStatus::STATUS_UNINITIALIZED:
    case OperationalStatus::STATUS_CODEC_REINIT_PENDING:
    // Not an error.
    // TODO(crbug.com/1015467): As an optimization, signal the client to pause
    // sending more frames until the state becomes STATUS_INITIALIZED again.
    case OperationalStatus::STATUS_INITIALIZED:
      break;
    case OperationalStatus::STATUS_INVALID_CONFIGURATION:
    case OperationalStatus::STATUS_UNSUPPORTED_CODEC:
    case OperationalStatus::STATUS_CODEC_INIT_FAILED:
    case OperationalStatus::STATUS_CODEC_RUNTIME_ERROR:
      ReportError(SessionError::ENCODING_ERROR);
      break;
  }
}

media::VideoEncodeAccelerator::SupportedProfiles
Session::GetSupportedVeaProfiles() {
  return supported_profiles_;
}

void Session::CreateVideoEncodeAccelerator(
    const media::cast::ReceiveVideoEncodeAcceleratorCallback& callback) {
  if (callback.is_null())
    return;
  std::unique_ptr<media::VideoEncodeAccelerator> mojo_vea;
  if (gpu_ && gpu_channel_host_ && !supported_profiles_.empty()) {
    if (!vea_provider_) {
      gpu_->CreateVideoEncodeAcceleratorProvider(
          vea_provider_.BindNewPipeAndPassReceiver());
    }
    mojo::PendingRemote<media::mojom::VideoEncodeAccelerator> vea;
    vea_provider_->CreateVideoEncodeAccelerator(
        vea.InitWithNewPipeAndPassReceiver());
    // std::make_unique doesn't work to create a unique pointer of the subclass.
    mojo_vea.reset(new media::MojoVideoEncodeAccelerator(std::move(vea),
                                                         supported_profiles_));
  }
  callback.Run(base::ThreadTaskRunnerHandle::Get(), std::move(mojo_vea));
}

void Session::CreateVideoEncodeMemory(
    size_t size,
    const media::cast::ReceiveVideoEncodeMemoryCallback& callback) {
  DVLOG(1) << __func__;

  base::UnsafeSharedMemoryRegion buf =
      base::UnsafeSharedMemoryRegion::Create(size);

  if (!buf.IsValid())
    LOG(WARNING) << "Browser failed to allocate shared memory.";

  callback.Run(std::move(buf));
}

void Session::OnTransportStatusChanged(CastTransportStatus status) {
  DVLOG(1) << __func__ << ": status=" << status;
  switch (status) {
    case CastTransportStatus::TRANSPORT_STREAM_UNINITIALIZED:
    case CastTransportStatus::TRANSPORT_STREAM_INITIALIZED:
      return;  // Not errors, do nothing.
    case CastTransportStatus::TRANSPORT_INVALID_CRYPTO_CONFIG:
    case CastTransportStatus::TRANSPORT_SOCKET_ERROR:
      ReportError(SessionError::CAST_TRANSPORT_ERROR);
      break;
  }
}

void Session::OnLoggingEventsReceived(
    std::unique_ptr<std::vector<FrameEvent>> frame_events,
    std::unique_ptr<std::vector<PacketEvent>> packet_events) {
  DCHECK(cast_environment_);
  cast_environment_->logger()->DispatchBatchOfEvents(std::move(frame_events),
                                                     std::move(packet_events));
}

void Session::OnAnswer(const std::vector<FrameSenderConfig>& audio_configs,
                       const std::vector<FrameSenderConfig>& video_configs,
                       const ReceiverResponse& response) {
  if (state_ == STOPPED)
    return;

  if (!response.answer || response.type == ResponseType::UNKNOWN) {
    ReportError(SessionError::ANSWER_TIME_OUT);
    return;
  }

  DCHECK_EQ(ResponseType::ANSWER, response.type);

  if (response.result != "ok") {
    ReportError(SessionError::ANSWER_NOT_OK);
    return;
  }

  const Answer& answer = *response.answer;
  const std::string cast_mode =
      (state_ == MIRRORING ? "mirroring" : "remoting");
  if (answer.cast_mode != cast_mode) {
    ReportError(SessionError::ANSWER_MISMATCHED_CAST_MODE);
    return;
  }

  if (answer.send_indexes.size() != answer.ssrcs.size()) {
    ReportError(SessionError::ANSWER_MISMATCHED_SSRC_LENGTH);
    return;
  }

  // Select Audio/Video config from ANSWER.
  bool has_audio = false;
  bool has_video = false;
  FrameSenderConfig audio_config;
  FrameSenderConfig video_config;
  const int video_start_idx = audio_configs.size();
  const int video_idx_bound = video_configs.size() + video_start_idx;
  for (size_t i = 0; i < answer.send_indexes.size(); ++i) {
    if (answer.send_indexes[i] < 0 ||
        answer.send_indexes[i] >= video_idx_bound) {
      ReportError(SessionError::ANSWER_SELECT_INVALID_INDEX);
      return;
    }
    if (answer.send_indexes[i] < video_start_idx) {
      // Audio
      if (has_audio) {
        ReportError(SessionError::ANSWER_SELECT_MULTIPLE_AUDIO);
        return;
      }
      audio_config = audio_configs[answer.send_indexes[i]];
      audio_config.receiver_ssrc = answer.ssrcs[i];
      has_audio = true;
    } else {
      // Video
      if (has_video) {
        ReportError(SessionError::ANSWER_SELECT_MULTIPLE_VIDEO);
        return;
      }
      video_config = video_configs[answer.send_indexes[i] - video_start_idx];
      video_config.receiver_ssrc = answer.ssrcs[i];
      video_config.video_codec_params.number_of_encode_threads =
          NumberOfEncodeThreads();
      has_video = true;
    }
  }
  if (!has_audio && !has_video) {
    ReportError(SessionError::ANSWER_NO_AUDIO_OR_VIDEO);
    return;
  }

  // Start streaming.
  const bool initially_starting_session =
      !audio_encode_thread_ && !video_encode_thread_;
  if (initially_starting_session) {
    audio_encode_thread_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    video_encode_thread_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }
  cast_environment_ = new media::cast::CastEnvironment(
      base::DefaultTickClock::GetInstance(),
      base::ThreadTaskRunnerHandle::Get(), audio_encode_thread_,
      video_encode_thread_);
  auto udp_client = std::make_unique<UdpSocketClient>(
      net::IPEndPoint(session_params_.receiver_address, answer.udp_port),
      network_context_.get(),
      base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                     SessionError::CAST_TRANSPORT_ERROR));
  cast_transport_ = media::cast::CastTransport::Create(
      cast_environment_->Clock(), kSendEventsInterval,
      std::make_unique<TransportClient>(this), std::move(udp_client),
      base::ThreadTaskRunnerHandle::Get());

  if (state_ == REMOTING) {
    DCHECK(media_remoter_);
    DCHECK(audio_config.rtp_payload_type == RtpPayloadType::REMOTE_AUDIO ||
           video_config.rtp_payload_type == RtpPayloadType::REMOTE_VIDEO);
    media_remoter_->StartRpcMessaging(cast_environment_, cast_transport_.get(),
                                      audio_config, video_config);
  } else /* MIRRORING */ {
    if (has_audio) {
      auto audio_sender = std::make_unique<media::cast::AudioSender>(
          cast_environment_, audio_config,
          base::BindOnce(&Session::OnEncoderStatusChange,
                         weak_factory_.GetWeakPtr()),
          cast_transport_.get());
      audio_stream_ = std::make_unique<AudioRtpStream>(
          std::move(audio_sender), weak_factory_.GetWeakPtr());
      DCHECK(!audio_capturing_callback_);
      // TODO(crbug.com/1015467): Eliminate the thread hops. The audio data is
      // thread-hopped from the audio thread, and later thread-hopped again to
      // the encoding thread.
      audio_capturing_callback_ = std::make_unique<AudioCapturingCallback>(
          media::BindToCurrentLoop(base::BindRepeating(
              &AudioRtpStream::InsertAudio, audio_stream_->AsWeakPtr())),
          base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                         SessionError::AUDIO_CAPTURE_ERROR));
      audio_input_device_ = new media::AudioInputDevice(
          std::make_unique<CapturedAudioInput>(base::BindRepeating(
              &Session::CreateAudioStream, base::Unretained(this))),
          media::AudioInputDevice::Purpose::kLoopback);
      audio_input_device_->Initialize(mirror_settings_.GetAudioCaptureParams(),
                                      audio_capturing_callback_.get());
      audio_input_device_->Start();
    }

    if (has_video) {
      auto video_sender = std::make_unique<media::cast::VideoSender>(
          cast_environment_, video_config,
          base::BindRepeating(&Session::OnEncoderStatusChange,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&Session::CreateVideoEncodeAccelerator,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&Session::CreateVideoEncodeMemory,
                              weak_factory_.GetWeakPtr()),
          cast_transport_.get(),
          base::BindRepeating(&Session::SetTargetPlayoutDelay,
                              weak_factory_.GetWeakPtr()));
      video_stream_ = std::make_unique<VideoRtpStream>(
          std::move(video_sender), weak_factory_.GetWeakPtr());
      if (!video_capture_client_) {
        mojo::PendingRemote<media::mojom::VideoCaptureHost> video_host;
        resource_provider_->GetVideoCaptureHost(
            video_host.InitWithNewPipeAndPassReceiver());
        video_capture_client_ = std::make_unique<VideoCaptureClient>(
            mirror_settings_.GetVideoCaptureParams(), std::move(video_host));
        video_capture_client_->Start(
            base::BindRepeating(&VideoRtpStream::InsertVideoFrame,
                                video_stream_->AsWeakPtr()),
            base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                           SessionError::VIDEO_CAPTURE_ERROR));
      } else {
        video_capture_client_->Resume(base::BindRepeating(
            &VideoRtpStream::InsertVideoFrame, video_stream_->AsWeakPtr()));
      }
    }
    if (media_remoter_)
      media_remoter_->OnMirroringResumed();
  }

  DCHECK(session_monitor_.has_value());
  const SessionMonitor::SessionType session_type =
      (has_audio && has_video)
          ? SessionMonitor::AUDIO_AND_VIDEO
          : has_audio ? SessionMonitor::AUDIO_ONLY : SessionMonitor::VIDEO_ONLY;
  std::unique_ptr<WifiStatusMonitor> wifi_status_monitor;
  if (answer.supports_get_status) {
    wifi_status_monitor =
        std::make_unique<WifiStatusMonitor>(&message_dispatcher_);
    // Nest Hub devices do not support remoting despite having a relatively new
    // build version, so we cannot filter with
    // NeedsWorkaroundForOlder1DotXVersions() here.
    if (initially_starting_session &&
        (base::StartsWith(session_params_.receiver_model_name, "Chromecast",
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(session_params_.receiver_model_name, "Eureka Dongle",
                          base::CompareCase::SENSITIVE))) {
      QueryCapabilitiesForRemoting();
    }
  }
  session_monitor_->StartStreamingSession(cast_environment_,
                                          std::move(wifi_status_monitor),
                                          session_type, state_ == REMOTING);

  if (initially_starting_session && observer_)
    observer_->DidStart();
}

void Session::OnResponseParsingError(const std::string& error_message) {
  // TODO(crbug.com/1015467): Log the |error_message| in the mirroring logs.
}

void Session::CreateAudioStream(
    mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
    const media::AudioParameters& params,
    uint32_t shared_memory_count) {
  resource_provider_->CreateAudioStream(std::move(client), params,
                                        shared_memory_count);
}

void Session::SetTargetPlayoutDelay(base::TimeDelta playout_delay) {
  if (audio_stream_)
    audio_stream_->SetTargetPlayoutDelay(playout_delay);
  if (video_stream_)
    video_stream_->SetTargetPlayoutDelay(playout_delay);
}

void Session::CreateAndSendOffer() {
  DCHECK(state_ != STOPPED);

  // The random AES key and initialization vector pair used by all streams in
  // this session.
  const std::string aes_key = MakeRandomString(16);  // AES-128.
  const std::string aes_iv = MakeRandomString(16);   // AES has 128-bit blocks.
  std::vector<FrameSenderConfig> audio_configs;
  std::vector<FrameSenderConfig> video_configs;

  // Generate stream list with supported audio / video configs.
  base::Value::ListStorage stream_list;
  int stream_index = 0;
  if (session_params_.type != SessionType::VIDEO_ONLY) {
    const int32_t audio_ssrc = base::RandInt(kAudioSsrcMin, kAudioSsrcMax);
    if (state_ == MIRRORING) {
      FrameSenderConfig config = MirrorSettings::GetDefaultAudioConfig(
          RtpPayloadType::AUDIO_OPUS, Codec::CODEC_AUDIO_OPUS);
      AddSenderConfig(audio_ssrc, config, aes_key, aes_iv, session_params_,
                      &audio_configs);
      AddStreamObject(stream_index++, "OPUS", audio_configs.back(),
                      mirror_settings_, &stream_list);
    } else /* REMOTING */ {
      FrameSenderConfig config = MirrorSettings::GetDefaultAudioConfig(
          RtpPayloadType::REMOTE_AUDIO, Codec::CODEC_AUDIO_REMOTE);
      AddSenderConfig(audio_ssrc, config, aes_key, aes_iv, session_params_,
                      &audio_configs);
      AddStreamObject(stream_index++, "REMOTE_AUDIO", audio_configs.back(),
                      mirror_settings_, &stream_list);
    }
  }
  if (session_params_.type != SessionType::AUDIO_ONLY) {
    const int32_t video_ssrc = base::RandInt(kVideoSsrcMin, kVideoSsrcMax);
    if (state_ == MIRRORING) {
      if (IsHardwareVP8EncodingSupported(GetSupportedVeaProfiles())) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_VP8, Codec::CODEC_VIDEO_VP8);
        config.use_external_encoder = true;
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "VP8", video_configs.back(),
                        mirror_settings_, &stream_list);
      }
      if (IsHardwareH264EncodingSupported(GetSupportedVeaProfiles())) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_H264, Codec::CODEC_VIDEO_H264);
        config.use_external_encoder = true;
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "H264", video_configs.back(),
                        mirror_settings_, &stream_list);
      }
      if (video_configs.empty()) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_VP8, Codec::CODEC_VIDEO_VP8);
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "VP8", video_configs.back(),
                        mirror_settings_, &stream_list);
      }
    } else /* REMOTING */ {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::REMOTE_VIDEO, Codec::CODEC_VIDEO_REMOTE);
      AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                      &video_configs);
      AddStreamObject(stream_index++, "REMOTE_VIDEO", video_configs.back(),
                      mirror_settings_, &stream_list);
    }
  }
  DCHECK(!audio_configs.empty() || !video_configs.empty());

  // Assemble the OFFER message.
  base::Value offer(base::Value::Type::DICTIONARY);
  offer.SetKey("castMode",
               base::Value(state_ == MIRRORING ? "mirroring" : "remoting"));
  offer.SetKey("receiverGetStatus", base::Value(true));
  offer.SetKey("supportedStreams", base::Value(stream_list));

  const int32_t sequence_number = message_dispatcher_.GetNextSeqNumber();
  base::Value offer_message(base::Value::Type::DICTIONARY);
  offer_message.SetKey("type", base::Value("OFFER"));
  offer_message.SetKey("seqNum", base::Value(sequence_number));
  offer_message.SetKey("offer", std::move(offer));

  mojom::CastMessagePtr message_to_receiver = mojom::CastMessage::New();
  message_to_receiver->message_namespace = mojom::kWebRtcNamespace;
  const bool did_serialize_offer = base::JSONWriter::Write(
      offer_message, &message_to_receiver->json_format_data);
  DCHECK(did_serialize_offer);

  message_dispatcher_.RequestReply(
      std::move(message_to_receiver), ResponseType::ANSWER, sequence_number,
      kOfferAnswerExchangeTimeout,
      base::BindOnce(&Session::OnAnswer, base::Unretained(this), audio_configs,
                     video_configs));
}

void Session::ConnectToRemotingSource(
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
  resource_provider_->ConnectToRemotingSource(std::move(remoter),
                                              std::move(receiver));
}

void Session::RequestRemotingStreaming() {
  DCHECK(media_remoter_);
  DCHECK_EQ(MIRRORING, state_);
  if (video_capture_client_)
    video_capture_client_->Pause();
  StopStreaming();
  state_ = REMOTING;
  CreateAndSendOffer();
}

void Session::RestartMirroringStreaming() {
  if (state_ != REMOTING)
    return;
  StopStreaming();
  state_ = MIRRORING;
  CreateAndSendOffer();
}

void Session::QueryCapabilitiesForRemoting() {
  DCHECK(!media_remoter_);
  const int32_t sequence_number = message_dispatcher_.GetNextSeqNumber();
  base::Value query(base::Value::Type::DICTIONARY);
  query.SetKey("type", base::Value("GET_CAPABILITIES"));
  query.SetKey("seqNum", base::Value(sequence_number));

  mojom::CastMessagePtr query_message = mojom::CastMessage::New();
  query_message->message_namespace = mojom::kWebRtcNamespace;
  const bool did_serialize_query =
      base::JSONWriter::Write(query, &query_message->json_format_data);
  DCHECK(did_serialize_query);
  message_dispatcher_.RequestReply(
      std::move(query_message), ResponseType::CAPABILITIES_RESPONSE,
      sequence_number, kGetCapabilitiesTimeout,
      base::BindOnce(&Session::OnCapabilitiesResponse, base::Unretained(this)));
}

void Session::OnCapabilitiesResponse(const ReceiverResponse& response) {
  if (!response.capabilities || response.type == ResponseType::UNKNOWN) {
    VLOG(1) << "Receiver doens't support GET_CAPABILITIES. Remoting disabled.";
    return;
  }
  if (response.result != "ok") {
    VLOG(1) << "Bad CAPABILITIES_RESPONSE. Remoting disabled.";
    if (response.error) {
      VLOG(1) << "error code=" << response.error->code
              << " description=" << response.error->description
              << " details=" << response.error->details;
    }
    return;
  }
  const std::vector<std::string>& caps = response.capabilities->media_caps;
  const std::string receiver_build_version =
      session_monitor_.has_value() ? session_monitor_->GetReceiverBuildVersion()
                                   : "";
  const std::string receiver_name =
      session_monitor_.has_value() ? session_monitor_->receiver_name() : "";
  media_remoter_ = std::make_unique<MediaRemoter>(
      this,
      ToRemotingSinkMetadata(caps, receiver_name, session_params_,
                             receiver_build_version),
      &message_dispatcher_);
}

}  // namespace mirroring
