// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/discovery/media_sink_service_base.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service.h"
#include "chrome/common/media_router/media_sink.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/logger.h"
#include "components/net_log/chrome_net_log.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_errors.h"

namespace {

media_router::MediaSinkInternal CreateCastSinkFromDialSink(
    const media_router::MediaSinkInternal& dial_sink) {
  const std::string& unique_id = dial_sink.sink().id();
  const std::string& friendly_name = dial_sink.sink().name();
  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::CAST);

  media_router::CastSinkExtraData extra_data;
  extra_data.ip_endpoint =
      net::IPEndPoint(dial_sink.dial_data().ip_address,
                      media_router::CastMediaSinkServiceImpl::kCastControlPort);
  extra_data.model_name = dial_sink.dial_data().model_name;
  extra_data.discovered_by_dial = true;
  extra_data.capabilities = cast_channel::CastDeviceCapability::NONE;

  return media_router::MediaSinkInternal(sink, extra_data);
}

void RecordError(cast_channel::ChannelError channel_error,
                 cast_channel::LastError last_error) {
  media_router::MediaRouterChannelError error_code =
      media_router::MediaRouterChannelError::UNKNOWN;

  switch (channel_error) {
    // TODO(crbug.com/767204): Add in errors for transient socket and timeout
    // errors, but only after X number of occurences.
    case cast_channel::ChannelError::UNKNOWN:
      error_code = media_router::MediaRouterChannelError::UNKNOWN;
      break;
    case cast_channel::ChannelError::AUTHENTICATION_ERROR:
      error_code = media_router::MediaRouterChannelError::AUTHENTICATION;
      break;
    case cast_channel::ChannelError::CONNECT_ERROR:
      error_code = media_router::MediaRouterChannelError::CONNECT;
      break;
    case cast_channel::ChannelError::CONNECT_TIMEOUT:
      error_code = media_router::MediaRouterChannelError::CONNECT_TIMEOUT;
      break;
    case cast_channel::ChannelError::PING_TIMEOUT:
      error_code = media_router::MediaRouterChannelError::PING_TIMEOUT;
      break;
    default:
      // Do nothing and let the standard launch failure issue surface.
      break;
  }

  // If we have details, we may override the generic error codes set above.
  // TODO(crbug.com/767204): Expand and refine below as we see more actual
  // reports.

  // General certificate errors
  if ((last_error.challenge_reply_error ==
           cast_channel::ChallengeReplyError::PEER_CERT_EMPTY ||
       last_error.challenge_reply_error ==
           cast_channel::ChallengeReplyError::FINGERPRINT_NOT_FOUND ||
       last_error.challenge_reply_error ==
           cast_channel::ChallengeReplyError::CERT_PARSING_FAILED ||
       last_error.challenge_reply_error ==
           cast_channel::ChallengeReplyError::CANNOT_EXTRACT_PUBLIC_KEY) ||
      (last_error.net_return_value <=
           net::ERR_CERT_COMMON_NAME_INVALID &&  // CERT_XXX errors
       last_error.net_return_value > net::ERR_CERT_END) ||
      last_error.channel_event ==
          cast_channel::ChannelEvent::SSL_SOCKET_CONNECT_FAILED ||
      last_error.channel_event ==
          cast_channel::ChannelEvent::SEND_AUTH_CHALLENGE_FAILED ||
      last_error.channel_event ==
          cast_channel::ChannelEvent::AUTH_CHALLENGE_REPLY_INVALID) {
    error_code = media_router::MediaRouterChannelError::GENERAL_CERTIFICATE;
  }

  // Certificate timing errors
  if (last_error.channel_event ==
          cast_channel::ChannelEvent::SSL_CERT_EXCESSIVE_LIFETIME ||
      last_error.net_return_value == net::ERR_CERT_DATE_INVALID) {
    error_code = media_router::MediaRouterChannelError::CERTIFICATE_TIMING;
  }

  // Network/firewall access denied
  if (last_error.net_return_value == net::ERR_NETWORK_ACCESS_DENIED) {
    error_code = media_router::MediaRouterChannelError::NETWORK;
  }

  // Authentication errors (assumed active ssl manipulation)
  if (last_error.challenge_reply_error ==
          cast_channel::ChallengeReplyError::CERT_NOT_SIGNED_BY_TRUSTED_CA ||
      last_error.challenge_reply_error ==
          cast_channel::ChallengeReplyError::SIGNED_BLOBS_MISMATCH) {
    error_code = media_router::MediaRouterChannelError::AUTHENTICATION;
  }

  media_router::CastAnalytics::RecordDeviceChannelError(error_code);
}

}  // namespace

namespace media_router {

namespace {

bool IsNetworkIdUnknownOrDisconnected(const std::string& network_id) {
  return network_id == DiscoveryNetworkMonitor::kNetworkIdUnknown ||
         network_id == DiscoveryNetworkMonitor::kNetworkIdDisconnected;
}

}  // namespace

// static
const net::BackoffEntry::Policy
    CastMediaSinkServiceImpl::kDefaultBackoffPolicy = {
        // Number of initial errors (in sequence) to ignore before going into
        // exponential backoff.
        0,

        // Initial delay (in ms) once backoff starts. It should be longer than
        // Cast
        // socket's liveness timeout |kConnectLivenessTimeoutSecs| (10 seconds).
        15 * 1000,  // 15 seconds

        // Factor by which the delay will be multiplied on each subsequent
        // failure.
        1.0,

        // Fuzzing percentage: 50% will spread delays randomly between 50%--100%
        // of
        // the nominal time.
        0.5,  // 50%

        // Maximum delay (in ms) during exponential backoff.
        30 * 1000,  // 30 seconds

        // Time to keep an entry from being discarded even when it has no
        // significant state, -1 to never discard. (Not applicable.)
        -1,

        // False means that initial_delay_ms is the first delay once we start
        // exponential backoff, i.e., there is no delay after subsequent
        // successful
        // requests.
        false,
};

constexpr int CastMediaSinkServiceImpl::kDefaultMaxRetryAttempts;
constexpr char const CastMediaSinkServiceImpl::kParamNameInitialDelayMS[];
constexpr char const CastMediaSinkServiceImpl::kParamNameMaxRetryAttempts[];
constexpr char const CastMediaSinkServiceImpl::kParamNameExponential[];

CastMediaSinkServiceImpl::CastMediaSinkServiceImpl(
    const OnSinksDiscoveredCallback& callback,
    cast_channel::CastSocketService* cast_socket_service,
    DiscoveryNetworkMonitor* network_monitor,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : MediaSinkServiceBase(callback),
      cast_socket_service_(cast_socket_service),
      network_monitor_(network_monitor),
      backoff_policy_(kDefaultBackoffPolicy),
      task_runner_(task_runner),
      net_log_(g_browser_process->net_log()),
      clock_(new base::DefaultClock()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(cast_socket_service_);
  DCHECK(network_monitor_);
  network_monitor_->AddObserver(this);
  cast_socket_service_->AddObserver(this);

  CastMediaSinkServiceImpl::InitRetryParameters(&backoff_policy_,
                                                &max_retry_attempts_);
}

CastMediaSinkServiceImpl::~CastMediaSinkServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cast_channel::CastSocketService::GetInstance()->RemoveObserver(this);
  network_monitor_->RemoveObserver(this);
}

void CastMediaSinkServiceImpl::SetTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void CastMediaSinkServiceImpl::SetClockForTest(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

// MediaSinkService implementation
void CastMediaSinkServiceImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaSinkServiceBase::StartTimer();
}

void CastMediaSinkServiceImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaSinkServiceBase::StopTimer();
}

void CastMediaSinkServiceImpl::OnFetchCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_sinks_.clear();

  // Copy cast sink from |current_sinks_map_| to |current_sinks_|.
  for (const auto& sink_it : current_sinks_map_) {
    DVLOG(2) << "Discovered by "
             << (sink_it.second.cast_data().discovered_by_dial ? "DIAL"
                                                               : "mDNS")
             << " [name]: " << sink_it.second.sink().name()
             << " [ip_endpoint]: "
             << sink_it.second.cast_data().ip_endpoint.ToString();
    current_sinks_.insert(sink_it.second);
  }

  MediaSinkServiceBase::OnFetchCompleted();
}

void CastMediaSinkServiceImpl::RecordDeviceCounts() {
  metrics_.RecordDeviceCountsIfNeeded(current_sinks_.size(),
                                      known_ip_endpoints_.size());
}

void CastMediaSinkServiceImpl::OpenChannels(
    std::vector<MediaSinkInternal> cast_sinks,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  known_ip_endpoints_.clear();

  for (const auto& cast_sink : cast_sinks) {
    const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
    known_ip_endpoints_.insert(ip_endpoint);
    OpenChannel(ip_endpoint, cast_sink,
                base::MakeUnique<net::BackoffEntry>(&backoff_policy_),
                sink_source);
  }

  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnError(const cast_channel::CastSocket& socket,
                                       cast_channel::ChannelError error_state) {
  DVLOG(1) << "OnError [ip_endpoint]: " << socket.ip_endpoint().ToString()
           << " [error_state]: "
           << cast_channel::ChannelErrorToString(error_state)
           << " [channel_id]: " << socket.id();

  cast_channel::LastError last_error =
      cast_socket_service_->GetLogger()->GetLastError(socket.id());
  RecordError(error_state, last_error);

  net::IPEndPoint ip_endpoint = socket.ip_endpoint();
  // Need a PostTask() here because RemoveSocket() will release the memory of
  // |socket|. Need to make sure all tasks on |socket| finish before deleting
  // the object.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&cast_channel::CastSocketService::RemoveSocket),
          base::Unretained(cast_socket_service_), socket.id()));

  // No op if socket is not opened yet. OnChannelOpened will handle this error.
  if (socket.ready_state() == cast_channel::ReadyState::CONNECTING) {
    DVLOG(2) << "Opening socket is pending, no-op... "
             << ip_endpoint.ToString();
    return;
  }

  std::unique_ptr<net::BackoffEntry> backoff_entry(
      new net::BackoffEntry(&backoff_policy_));

  DVLOG(2) << "OnError starts reopening cast channel: "
           << ip_endpoint.ToString();
  // Find existing cast sink from |current_sinks_map_|.
  auto cast_sink_it = current_sinks_map_.find(ip_endpoint.address());
  if (cast_sink_it != current_sinks_map_.end()) {
    OnChannelErrorMayRetry(cast_sink_it->second, std::move(backoff_entry),
                           error_state, SinkSource::kConnectionRetry);
    return;
  }

  DVLOG(2) << "Cannot find existing cast sink. Skip reopen cast channel: "
           << ip_endpoint.ToString();
}

void CastMediaSinkServiceImpl::OnMessage(
    const cast_channel::CastSocket& socket,
    const cast_channel::CastMessage& message) {}

void CastMediaSinkServiceImpl::OnNetworksChanged(
    const std::string& network_id) {
  std::string last_network_id = current_network_id_;
  current_network_id_ = network_id;
  if (IsNetworkIdUnknownOrDisconnected(network_id)) {
    if (!IsNetworkIdUnknownOrDisconnected(last_network_id)) {
      // Collect current sinks even if OnFetchCompleted hasn't collected the
      // latest sinks.
      std::vector<MediaSinkInternal> current_sinks;
      for (const auto& sink_it : current_sinks_map_) {
        current_sinks.push_back(sink_it.second);
      }
      sink_cache_[last_network_id] = std::move(current_sinks);
    }
    return;
  }
  auto cache_entry = sink_cache_.find(network_id);
  // Check if we have any cached sinks for this network ID.
  if (cache_entry == sink_cache_.end())
    return;

  metrics_.RecordCachedSinksAvailableCount(cache_entry->second.size());

  DVLOG(2) << "Cache restored " << cache_entry->second.size()
           << " sink(s) for network " << network_id;
  OpenChannels(cache_entry->second, SinkSource::kNetworkCache);
}

void CastMediaSinkServiceImpl::OpenChannel(
    const net::IPEndPoint& ip_endpoint,
    const MediaSinkInternal& cast_sink,
    std::unique_ptr<net::BackoffEntry> backoff_entry,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_for_open_ip_endpoints_.insert(ip_endpoint).second) {
    DVLOG(2) << "Pending opening request for " << ip_endpoint.ToString()
             << " name: " << cast_sink.sink().name();
    return;
  }

  DVLOG(2) << "Start OpenChannel " << ip_endpoint.ToString()
           << " name: " << cast_sink.sink().name();

  cast_socket_service_->OpenSocket(
      ip_endpoint, net_log_,
      base::BindOnce(&CastMediaSinkServiceImpl::OnChannelOpened, AsWeakPtr(),
                     cast_sink, std::move(backoff_entry), sink_source,
                     clock_->Now()));
}

void CastMediaSinkServiceImpl::OnChannelOpened(
    const MediaSinkInternal& cast_sink,
    std::unique_ptr<net::BackoffEntry> backoff_entry,
    SinkSource sink_source,
    base::Time start_time,
    cast_channel::CastSocket* socket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket);

  pending_for_open_ip_endpoints_.erase(cast_sink.cast_data().ip_endpoint);
  bool succeeded = socket->error_state() == cast_channel::ChannelError::NONE;
  if (backoff_entry)
    backoff_entry->InformOfRequest(succeeded);

  CastAnalytics::RecordDeviceChannelOpenDuration(succeeded,
                                                 clock_->Now() - start_time);

  if (succeeded) {
    OnChannelOpenSucceeded(cast_sink, socket, sink_source);
  } else {
    OnChannelErrorMayRetry(cast_sink, std::move(backoff_entry),
                           socket->error_state(), sink_source);
  }
}

void CastMediaSinkServiceImpl::OnChannelErrorMayRetry(
    MediaSinkInternal cast_sink,
    std::unique_ptr<net::BackoffEntry> backoff_entry,
    cast_channel::ChannelError error_state,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
  OnChannelOpenFailed(ip_endpoint);

  if (!backoff_entry || backoff_entry->failure_count() > max_retry_attempts_) {
    DVLOG(1) << "Fail to open channel after all retry attempts: "
             << ip_endpoint.ToString() << " [error_state]: "
             << cast_channel::ChannelErrorToString(error_state);

    CastAnalytics::RecordCastChannelConnectResult(false);
    return;
  }

  const base::TimeDelta delay = backoff_entry->GetTimeUntilRelease();
  DVLOG(2) << "Try to reopen: " << ip_endpoint.ToString() << " in ["
           << delay.InSeconds() << "] seconds"
           << " [Attempt]: " << backoff_entry->failure_count();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel, AsWeakPtr(),
                     ip_endpoint, std::move(cast_sink),
                     std::move(backoff_entry), sink_source),
      delay);
}

void CastMediaSinkServiceImpl::OnChannelOpenSucceeded(
    MediaSinkInternal cast_sink,
    cast_channel::CastSocket* socket,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket);

  CastAnalytics::RecordCastChannelConnectResult(true);

  media_router::CastSinkExtraData extra_data = cast_sink.cast_data();
  extra_data.capabilities = cast_channel::CastDeviceCapability::AUDIO_OUT;
  if (!socket->audio_only())
    extra_data.capabilities |= cast_channel::CastDeviceCapability::VIDEO_OUT;
  extra_data.cast_channel_id = socket->id();
  cast_sink.set_cast_data(extra_data);
  DVLOG(2) << "Ading sink to current_sinks_ [name]: "
           << cast_sink.sink().name();

  // Add or update existing cast sink.
  auto& ip_address = cast_sink.cast_data().ip_endpoint.address();
  auto sink_it = current_sinks_map_.find(ip_address);
  if (sink_it == current_sinks_map_.end()) {
    metrics_.RecordResolvedFromSource(sink_source);
  } else if (sink_it->second.cast_data().discovered_by_dial &&
             !cast_sink.cast_data().discovered_by_dial) {
    metrics_.RecordResolvedFromSource(SinkSource::kDialMdns);
  }
  current_sinks_map_[ip_address] = cast_sink;

  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnChannelOpenFailed(
    const net::IPEndPoint& ip_endpoint) {
  auto& ip_address = ip_endpoint.address();
  current_sinks_map_.erase(ip_address);
  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnDialSinkAdded(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net::IPEndPoint ip_endpoint(sink.dial_data().ip_address, kCastControlPort);

  if (base::ContainsKey(current_sinks_map_, ip_endpoint.address())) {
    DVLOG(2) << "Sink discovered by mDNS, skip adding [name]: "
             << sink.sink().name();
    metrics_.RecordResolvedFromSource(SinkSource::kMdnsDial);
    return;
  }

  // TODO(crbug.com/753175): Dual discovery should not try to open cast channel
  // for non-Cast device.
  OpenChannel(ip_endpoint, CreateCastSinkFromDialSink(sink),
              base::MakeUnique<net::BackoffEntry>(&backoff_policy_),
              SinkSource::kDial);
}

// static
void CastMediaSinkServiceImpl::InitRetryParameters(
    net::BackoffEntry::Policy* backoff_policy,
    int* max_retry_attempts) {
  DCHECK(backoff_policy);
  DCHECK(max_retry_attempts);

  *backoff_policy = kDefaultBackoffPolicy;
  if (!CastChannelRetryEnabled()) {
    *max_retry_attempts = 0;
    return;
  }

  *max_retry_attempts = base::GetFieldTrialParamByFeatureAsInt(
      kEnableCastChannelRetry, kParamNameMaxRetryAttempts,
      kDefaultMaxRetryAttempts);

  backoff_policy->initial_delay_ms = base::GetFieldTrialParamByFeatureAsInt(
      kEnableCastChannelRetry, kParamNameInitialDelayMS,
      kDefaultBackoffPolicy.initial_delay_ms);

  backoff_policy->multiply_factor = base::GetFieldTrialParamByFeatureAsDouble(
      kEnableCastChannelRetry, kParamNameExponential,
      kDefaultBackoffPolicy.multiply_factor);

  DVLOG(2) << "Retry strategy parameters "
           << " [initial_delay_ms]: " << backoff_policy->initial_delay_ms
           << " [max_retry_attempts]: " << *max_retry_attempts
           << " [exponential]: " << backoff_policy->multiply_factor;
}

void CastMediaSinkServiceImpl::AttemptConnection(
    const std::vector<MediaSinkInternal>& cast_sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& cast_sink : cast_sinks) {
    const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
    if (!base::ContainsKey(current_sinks_map_, ip_endpoint.address())) {
      OpenChannel(ip_endpoint, cast_sink,
                  base::MakeUnique<net::BackoffEntry>(&backoff_policy_),
                  SinkSource::kConnectionRetry);
    }
  }
}

}  // namespace media_router
