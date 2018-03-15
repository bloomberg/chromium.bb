// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/media_sink.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_channel_util.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/logger.h"
#include "components/net_log/chrome_net_log.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_errors.h"

namespace media_router {

namespace {

MediaSinkInternal CreateCastSinkFromDialSink(
    const MediaSinkInternal& dial_sink) {
  const std::string& dial_sink_id = dial_sink.sink().id();
  const std::string& friendly_name = dial_sink.sink().name();
  DCHECK_EQ("dial:", dial_sink_id.substr(0, 5))
      << "unexpected DIAL sink id " << dial_sink_id;

  // Replace the "dial:" prefix with "cast:".
  std::string sink_id = "cast:" + dial_sink_id.substr(5);

  // Note that the real sink icon will be determined later using information
  // from the opened cast channel.
  MediaSink sink(sink_id, friendly_name, SinkIconType::CAST,
                 MediaRouteProviderId::CAST);

  CastSinkExtraData extra_data;
  extra_data.ip_endpoint =
      net::IPEndPoint(dial_sink.dial_data().ip_address,
                      CastMediaSinkServiceImpl::kCastControlPort);
  extra_data.model_name = dial_sink.dial_data().model_name;
  extra_data.discovered_by_dial = true;
  extra_data.capabilities = cast_channel::CastDeviceCapability::NONE;

  return MediaSinkInternal(sink, extra_data);
}

void RecordError(cast_channel::ChannelError channel_error,
                 cast_channel::LastError last_error) {
  MediaRouterChannelError error_code = MediaRouterChannelError::UNKNOWN;

  switch (channel_error) {
    // TODO(crbug.com/767204): Add in errors for transient socket and timeout
    // errors, but only after X number of occurences.
    case cast_channel::ChannelError::UNKNOWN:
      error_code = MediaRouterChannelError::UNKNOWN;
      break;
    case cast_channel::ChannelError::AUTHENTICATION_ERROR:
      error_code = MediaRouterChannelError::AUTHENTICATION;
      break;
    case cast_channel::ChannelError::CONNECT_ERROR:
      error_code = MediaRouterChannelError::CONNECT;
      break;
    case cast_channel::ChannelError::CONNECT_TIMEOUT:
      error_code = MediaRouterChannelError::CONNECT_TIMEOUT;
      break;
    case cast_channel::ChannelError::PING_TIMEOUT:
      error_code = MediaRouterChannelError::PING_TIMEOUT;
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
    error_code = MediaRouterChannelError::GENERAL_CERTIFICATE;
  }

  // Certificate timing errors
  if (last_error.channel_event ==
          cast_channel::ChannelEvent::SSL_CERT_EXCESSIVE_LIFETIME ||
      last_error.net_return_value == net::ERR_CERT_DATE_INVALID) {
    error_code = MediaRouterChannelError::CERTIFICATE_TIMING;
  }

  // Network/firewall access denied
  if (last_error.net_return_value == net::ERR_NETWORK_ACCESS_DENIED) {
    error_code = MediaRouterChannelError::NETWORK;
  }

  // Authentication errors (assumed active ssl manipulation)
  if (last_error.challenge_reply_error ==
          cast_channel::ChallengeReplyError::CERT_NOT_SIGNED_BY_TRUSTED_CA ||
      last_error.challenge_reply_error ==
          cast_channel::ChallengeReplyError::SIGNED_BLOBS_MISMATCH) {
    error_code = MediaRouterChannelError::AUTHENTICATION;
  }

  CastAnalytics::RecordDeviceChannelError(error_code);
}

// Parameter names.
constexpr char kParamNameInitialDelayInMilliSeconds[] = "initial_delay_in_ms";
constexpr char kParamNameMaxRetryAttempts[] = "max_retry_attempts";
constexpr char kParamNameExponential[] = "exponential";
constexpr char kParamNameConnectTimeoutInSeconds[] =
    "connect_timeout_in_seconds";
constexpr char kParamNamePingIntervalInSeconds[] = "ping_interval_in_seconds";
constexpr char kParamNameLivenessTimeoutInSeconds[] =
    "liveness_timeout_in_seconds";
constexpr char kParamNameDynamicTimeoutDeltaInSeconds[] =
    "dynamic_timeout_delta_in_seconds";

// Default values if field trial parameter is not specified.
constexpr int kDefaultInitialDelayInMilliSeconds = 15 * 1000;  // 15 seconds
// TODO(zhaobin): Remove this when we switch to use max delay instead of max
// number of retry attempts to decide when to stop retry.
constexpr int kDefaultMaxRetryAttempts = 3;
constexpr double kDefaultExponential = 1.0;
constexpr int kDefaultConnectTimeoutInSeconds = 10;
constexpr int kDefaultPingIntervalInSeconds = 5;
constexpr int kDefaultLivenessTimeoutInSeconds =
    kDefaultPingIntervalInSeconds * 2;
constexpr int kDefaultDynamicTimeoutDeltaInSeconds = 0;

// Max allowed values
constexpr int kMaxConnectTimeoutInSeconds = 30;
constexpr int kMaxLivenessTimeoutInSeconds = 60;

// Max failure count allowed for a Cast channel.
constexpr int kMaxFailureCount = 100;

bool IsNetworkIdUnknownOrDisconnected(const std::string& network_id) {
  return network_id == DiscoveryNetworkMonitor::kNetworkIdUnknown ||
         network_id == DiscoveryNetworkMonitor::kNetworkIdDisconnected;
}

}  // namespace

// static
constexpr int CastMediaSinkServiceImpl::kMaxDialSinkFailureCount;

// static
SinkIconType CastMediaSinkServiceImpl::GetCastSinkIconType(
    uint8_t capabilities) {
  if (capabilities & cast_channel::CastDeviceCapability::VIDEO_OUT)
    return SinkIconType::CAST;

  return capabilities & cast_channel::CastDeviceCapability::MULTIZONE_GROUP
             ? SinkIconType::CAST_AUDIO_GROUP
             : SinkIconType::CAST_AUDIO;
}

CastMediaSinkServiceImpl::CastMediaSinkServiceImpl(
    const OnSinksDiscoveredCallback& callback,
    Observer* observer,
    cast_channel::CastSocketService* cast_socket_service,
    DiscoveryNetworkMonitor* network_monitor)
    : MediaSinkServiceBase(callback),
      observer_(observer),
      cast_socket_service_(cast_socket_service),
      network_monitor_(network_monitor),
      task_runner_(cast_socket_service_->task_runner()),
      clock_(base::DefaultClock::GetInstance()),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(cast_socket_service_);
  DCHECK(network_monitor_);

  retry_params_ = RetryParams::GetFromFieldTrialParam();
  open_params_ = OpenParams::GetFromFieldTrialParam();

  backoff_policy_ = {
      // Number of initial errors (in sequence) to ignore before going into
      // exponential backoff.
      0,

      // Initial delay (in ms) once backoff starts. It should be longer than
      // Cast
      // socket's liveness timeout |kConnectLivenessTimeoutSecs| (10 seconds).
      retry_params_.initial_delay_in_milliseconds,

      // Factor by which the delay will be multiplied on each subsequent
      // failure.
      retry_params_.multiply_factor,

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
}

CastMediaSinkServiceImpl::~CastMediaSinkServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_monitor_->RemoveObserver(this);
  cast_socket_service_->RemoveObserver(this);
}

void CastMediaSinkServiceImpl::SetClockForTest(base::Clock* clock) {
  clock_ = clock;
}

void CastMediaSinkServiceImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaSinkServiceBase::StartTimer();

  cast_socket_service_->AddObserver(this);

  // This call to |GetNetworkId| ensures that we get the current network ID at
  // least once during startup in case |AddObserver| occurs after the first
  // round of notifications has already been dispatched.
  network_monitor_->GetNetworkId(base::BindOnce(
      &CastMediaSinkServiceImpl::OnNetworksChanged, GetWeakPtr()));
  network_monitor_->AddObserver(this);
}

void CastMediaSinkServiceImpl::OnDiscoveryComplete() {
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
    std::string sink_id = sink_it.second.sink().id();
    current_sinks_.emplace(sink_id, sink_it.second);
  }

  MediaSinkServiceBase::OnDiscoveryComplete();
}

void CastMediaSinkServiceImpl::RecordDeviceCounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_.RecordDeviceCountsIfNeeded(current_sinks_.size(),
                                      known_ip_endpoints_.size());
}

void CastMediaSinkServiceImpl::OpenChannelsWithRandomizedDelay(
    const std::vector<MediaSinkInternal>& cast_sinks,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Add a random backoff between 0s to 5s before opening channels to prevent
  // different browser instances connecting to the same receiver at the same
  // time.
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(base::RandInt(0, 50) * 100);
  DVLOG(2) << "Open channels in [" << delay.InSeconds() << "] seconds";
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannels, GetWeakPtr(),
                     cast_sinks, sink_source),
      delay);
}

void CastMediaSinkServiceImpl::OpenChannels(
    const std::vector<MediaSinkInternal>& cast_sinks,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  known_ip_endpoints_.clear();

  for (const auto& cast_sink : cast_sinks) {
    const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
    known_ip_endpoints_.insert(ip_endpoint);
    OpenChannel(ip_endpoint, cast_sink, nullptr, sink_source);
  }

  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnError(const cast_channel::CastSocket& socket,
                                       cast_channel::ChannelError error_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  task_runner_->PostNonNestableTask(
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

  DVLOG(2) << "OnError starts reopening cast channel: "
           << ip_endpoint.ToString();
  // Find existing cast sink from |current_sinks_map_|.
  auto cast_sink_it = current_sinks_map_.find(ip_endpoint);
  if (cast_sink_it != current_sinks_map_.end()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel, GetWeakPtr(),
                       ip_endpoint, cast_sink_it->second, nullptr,
                       SinkSource::kConnectionRetry));
    // We erase the sink here so that OpenChannel would not find an existing
    // sink.
    // Note: a better longer term solution is to introduce a state field to the
    // sink. We would set it to ERROR here. In OpenChannel(), we would check
    // create a socket only if the state is not already CONNECTED.
    if (observer_)
      observer_->OnSinkRemoved(cast_sink_it->second);

    current_sinks_map_.erase(cast_sink_it);
    MediaSinkServiceBase::RestartTimer();
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Although DiscoveryNetworkMonitor guarantees this condition won't be true
  // from its Observer interface, the callback from |AddNetworkChangeObserver|
  // could cause this to happen.
  if (network_id == current_network_id_) {
    return;
  }
  std::string last_network_id = current_network_id_;
  current_network_id_ = network_id;
  dial_sink_failure_count_.clear();
  if (!IsNetworkIdUnknownOrDisconnected(last_network_id)) {
    // Collect current sinks even if OnFetchCompleted hasn't collected the
    // latest sinks.
    std::vector<MediaSinkInternal> current_sinks;
    for (const auto& sink_it : current_sinks_map_) {
      current_sinks.push_back(sink_it.second);
    }
    sink_cache_[last_network_id] = std::move(current_sinks);
  }

  // TODO(imcheng): Maybe this should clear |current_sinks_map_| and call
  // |RestartTimer()| so it is more responsive?
  if (IsNetworkIdUnknownOrDisconnected(network_id))
    return;

  auto cache_entry = sink_cache_.find(network_id);
  // Check if we have any cached sinks for this network ID.
  if (cache_entry == sink_cache_.end())
    return;

  metrics_.RecordCachedSinksAvailableCount(cache_entry->second.size());

  DVLOG(2) << "Cache restored " << cache_entry->second.size()
           << " sink(s) for network " << network_id;
  OpenChannelsWithRandomizedDelay(cache_entry->second,
                                  SinkSource::kNetworkCache);
}

cast_channel::CastSocketOpenParams
CastMediaSinkServiceImpl::CreateCastSocketOpenParams(
    const net::IPEndPoint& ip_endpoint) {
  int connect_timeout_in_seconds = open_params_.connect_timeout_in_seconds;
  int liveness_timeout_in_seconds = open_params_.liveness_timeout_in_seconds;
  int delta_in_seconds = open_params_.dynamic_timeout_delta_in_seconds;

  auto it = failure_count_map_.find(ip_endpoint);
  if (it != failure_count_map_.end()) {
    int failure_count = it->second;
    connect_timeout_in_seconds =
        std::min(connect_timeout_in_seconds + failure_count * delta_in_seconds,
                 kMaxConnectTimeoutInSeconds);
    liveness_timeout_in_seconds =
        std::min(liveness_timeout_in_seconds + failure_count * delta_in_seconds,
                 kMaxLivenessTimeoutInSeconds);
  }

  // TODO(crbug.com/814419): Switching cast socket implementation to use network
  // service will allow us to get back NetLog.
  return cast_channel::CastSocketOpenParams(
      ip_endpoint, nullptr,
      base::TimeDelta::FromSeconds(connect_timeout_in_seconds),
      base::TimeDelta::FromSeconds(liveness_timeout_in_seconds),
      base::TimeDelta::FromSeconds(open_params_.ping_interval_in_seconds),
      cast_channel::CastDeviceCapability::NONE);
}

void CastMediaSinkServiceImpl::OpenChannel(
    const net::IPEndPoint& ip_endpoint,
    const MediaSinkInternal& cast_sink,
    std::unique_ptr<net::BackoffEntry> backoff_entry,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!cast_channel::IsValidCastIPAddress(ip_endpoint.address()))
    return;

  // Erase the entry from |dial_sink_failure_count_| since the device is now
  // known to be a Cast device.
  if (sink_source != SinkSource::kDial)
    dial_sink_failure_count_.erase(ip_endpoint.address());

  if (base::ContainsKey(current_sinks_map_, ip_endpoint)) {
    DVLOG(2) << "A channel already exists for " << ip_endpoint.ToString();
    return;
  }

  if (!pending_for_open_ip_endpoints_.insert(ip_endpoint).second) {
    DVLOG(2) << "Pending opening request for " << ip_endpoint.ToString()
             << " name: " << cast_sink.sink().name();
    return;
  }

  DVLOG(2) << "Start OpenChannel " << ip_endpoint.ToString()
           << " name: " << cast_sink.sink().name();

  cast_channel::CastSocketOpenParams open_params =
      CreateCastSocketOpenParams(ip_endpoint);
  cast_socket_service_->OpenSocket(
      open_params,
      base::BindOnce(&CastMediaSinkServiceImpl::OnChannelOpened, GetWeakPtr(),
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
  if (sink_source == SinkSource::kDial)
    ++dial_sink_failure_count_[ip_endpoint.address()];

  int failure_count = ++failure_count_map_[ip_endpoint];
  failure_count_map_[ip_endpoint] = std::min(failure_count, kMaxFailureCount);

  if (!backoff_entry)
    backoff_entry = std::make_unique<net::BackoffEntry>(&backoff_policy_);

  if (backoff_entry->failure_count() >= retry_params_.max_retry_attempts) {
    DVLOG(1) << "Fail to open channel after all retry attempts: "
             << ip_endpoint.ToString() << " [error_state]: "
             << cast_channel::ChannelErrorToString(error_state);

    OnChannelOpenFailed(ip_endpoint, cast_sink);
    CastAnalytics::RecordCastChannelConnectResult(
        MediaRouterChannelConnectResults::FAILURE);
    return;
  }

  const base::TimeDelta delay = backoff_entry->GetTimeUntilRelease();
  DVLOG(2) << "Try to reopen: " << ip_endpoint.ToString() << " in ["
           << delay.InSeconds() << "] seconds"
           << " [Attempt]: " << backoff_entry->failure_count();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel, GetWeakPtr(),
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

  CastAnalytics::RecordCastChannelConnectResult(
      MediaRouterChannelConnectResults::SUCCESS);
  CastSinkExtraData extra_data = cast_sink.cast_data();
  // Manually set device capabilities for sinks discovered via DIAL as DIAL
  // discovery does not provide capability info.
  if (cast_sink.cast_data().discovered_by_dial) {
    extra_data.capabilities = cast_channel::CastDeviceCapability::AUDIO_OUT;
    if (!socket->audio_only())
      extra_data.capabilities |= cast_channel::CastDeviceCapability::VIDEO_OUT;

    // We can now set the proper icon type now that capabilities is determined.
    cast_sink.sink().set_icon_type(
        GetCastSinkIconType(extra_data.capabilities));
  }

  extra_data.cast_channel_id = socket->id();
  cast_sink.set_cast_data(extra_data);

  DVLOG(2) << "Adding sink to current_sinks_ [name]: "
           << cast_sink.sink().name();

  // Add or update existing cast sink.
  auto& ip_endpoint = cast_sink.cast_data().ip_endpoint;
  auto sink_it = current_sinks_map_.find(ip_endpoint);
  if (sink_it == current_sinks_map_.end()) {
    metrics_.RecordCastSinkDiscoverySource(sink_source);
    current_sinks_map_.emplace(ip_endpoint, cast_sink);
  } else {
    if (sink_it->second.cast_data().discovered_by_dial &&
        !cast_sink.cast_data().discovered_by_dial) {
      metrics_.RecordCastSinkDiscoverySource(SinkSource::kDialMdns);
    }
    sink_it->second = cast_sink;
  }

  // If the sink was under a different IP address previously, remove it from
  // |current_sinks_map_|.
  auto old_sink_it = std::find_if(
      current_sinks_map_.begin(), current_sinks_map_.end(),
      [&cast_sink, &ip_endpoint](
          const std::pair<net::IPEndPoint, MediaSinkInternal>& entry) {
        return !(entry.first == ip_endpoint) &&
               entry.second.sink().id() == cast_sink.sink().id();
      });
  if (old_sink_it != current_sinks_map_.end())
    current_sinks_map_.erase(old_sink_it);

  if (observer_)
    observer_->OnSinkAddedOrUpdated(cast_sink, socket);

  failure_count_map_.erase(ip_endpoint);
  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnChannelOpenFailed(
    const net::IPEndPoint& ip_endpoint,
    const MediaSinkInternal& sink) {
  // It is possible that the old sink in |current_sinks_map_| is replaced with
  // a new sink if a network change happened. Check the sink ID to make sure
  // this is the sink we want to erase.
  auto it = current_sinks_map_.find(ip_endpoint);
  if (it == current_sinks_map_.end() ||
      it->second.sink().id() != sink.sink().id())
    return;

  if (observer_)
    observer_->OnSinkRemoved(it->second);

  current_sinks_map_.erase(it);
  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnDialSinkAdded(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net::IPEndPoint ip_endpoint(sink.dial_data().ip_address, kCastControlPort);

  if (base::ContainsKey(current_sinks_map_, ip_endpoint)) {
    DVLOG(2) << "Sink discovered by mDNS, skip adding [name]: "
             << sink.sink().name();
    metrics_.RecordCastSinkDiscoverySource(SinkSource::kMdnsDial);
    return;
  }

  // TODO(crbug.com/753175): Dual discovery should not try to open cast channel
  // for non-Cast device.
  if (IsProbablyNonCastDevice(sink)) {
    DVLOG(2) << "Skip open channel for DIAL-discovered device because it "
             << "is probably not a Cast device: " << sink.sink().name();
    return;
  }

  OpenChannel(ip_endpoint, CreateCastSinkFromDialSink(sink), nullptr,
              SinkSource::kDial);
}

bool CastMediaSinkServiceImpl::IsProbablyNonCastDevice(
    const MediaSinkInternal& dial_sink) const {
  auto it = dial_sink_failure_count_.find(dial_sink.dial_data().ip_address);
  return it != dial_sink_failure_count_.end() &&
         it->second >= kMaxDialSinkFailureCount;
}

void CastMediaSinkServiceImpl::AttemptConnection(
    const std::vector<MediaSinkInternal>& cast_sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& cast_sink : cast_sinks) {
    const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
    OpenChannel(ip_endpoint, cast_sink, nullptr, SinkSource::kConnectionRetry);
  }
}

OnDialSinkAddedCallback CastMediaSinkServiceImpl::GetDialSinkAddedCallback() {
  return base::BindRepeating(&CastMediaSinkServiceImpl::OnDialSinkAdded,
                             base::Unretained(this));
}

CastMediaSinkServiceImpl::RetryParams::RetryParams()
    : initial_delay_in_milliseconds(kDefaultInitialDelayInMilliSeconds),
      max_retry_attempts(kDefaultMaxRetryAttempts),
      multiply_factor(kDefaultExponential) {}
CastMediaSinkServiceImpl::RetryParams::~RetryParams() = default;

bool CastMediaSinkServiceImpl::RetryParams::Validate() {
  if (max_retry_attempts < 0 || max_retry_attempts > 100) {
    return false;
  }
  if (initial_delay_in_milliseconds <= 0 ||
      initial_delay_in_milliseconds > 60 * 1000 /* 1 min */) {
    return false;
  }
  if (multiply_factor < 1.0 || multiply_factor > 5.0)
    return false;

  return true;
}

// static
CastMediaSinkServiceImpl::RetryParams
CastMediaSinkServiceImpl::RetryParams::GetFromFieldTrialParam() {
  RetryParams params;
  params.max_retry_attempts = base::GetFieldTrialParamByFeatureAsInt(
      kEnableCastDiscovery, kParamNameMaxRetryAttempts,
      kDefaultMaxRetryAttempts);
  params.initial_delay_in_milliseconds = base::GetFieldTrialParamByFeatureAsInt(
      kEnableCastDiscovery, kParamNameInitialDelayInMilliSeconds,
      kDefaultInitialDelayInMilliSeconds);
  params.multiply_factor = base::GetFieldTrialParamByFeatureAsDouble(
      kEnableCastDiscovery, kParamNameExponential, kDefaultExponential);

  DVLOG(2) << "Parameters: "
           << " [initial_delay_ms]: " << params.initial_delay_in_milliseconds
           << " [max_retry_attempts]: " << params.max_retry_attempts
           << " [exponential]: " << params.multiply_factor;

  if (!params.Validate())
    return RetryParams();

  return params;
}

CastMediaSinkServiceImpl::OpenParams::OpenParams()
    : connect_timeout_in_seconds(kDefaultConnectTimeoutInSeconds),
      ping_interval_in_seconds(kDefaultPingIntervalInSeconds),
      liveness_timeout_in_seconds(kDefaultLivenessTimeoutInSeconds),
      dynamic_timeout_delta_in_seconds(kDefaultDynamicTimeoutDeltaInSeconds) {}
CastMediaSinkServiceImpl::OpenParams::~OpenParams() = default;

bool CastMediaSinkServiceImpl::OpenParams::Validate() {
  if (connect_timeout_in_seconds <= 0 ||
      connect_timeout_in_seconds > kMaxConnectTimeoutInSeconds) {
    return false;
  }
  if (liveness_timeout_in_seconds <= 0 ||
      liveness_timeout_in_seconds > kMaxLivenessTimeoutInSeconds) {
    return false;
  }
  if (ping_interval_in_seconds <= 0 ||
      ping_interval_in_seconds > liveness_timeout_in_seconds) {
    return false;
  }
  if (dynamic_timeout_delta_in_seconds < 0 ||
      dynamic_timeout_delta_in_seconds > kMaxConnectTimeoutInSeconds) {
    return false;
  }

  return true;
}

// static
CastMediaSinkServiceImpl::OpenParams
CastMediaSinkServiceImpl::OpenParams::GetFromFieldTrialParam() {
  OpenParams params;
  params.connect_timeout_in_seconds = base::GetFieldTrialParamByFeatureAsInt(
      kEnableCastDiscovery, kParamNameConnectTimeoutInSeconds,
      kDefaultConnectTimeoutInSeconds);
  params.ping_interval_in_seconds = base::GetFieldTrialParamByFeatureAsInt(
      kEnableCastDiscovery, kParamNamePingIntervalInSeconds,
      kDefaultPingIntervalInSeconds);
  params.liveness_timeout_in_seconds = base::GetFieldTrialParamByFeatureAsInt(
      kEnableCastDiscovery, kParamNameLivenessTimeoutInSeconds,
      kDefaultLivenessTimeoutInSeconds);
  params.dynamic_timeout_delta_in_seconds =
      base::GetFieldTrialParamByFeatureAsInt(
          kEnableCastDiscovery, kParamNameDynamicTimeoutDeltaInSeconds,
          kDefaultDynamicTimeoutDeltaInSeconds);

  DVLOG(2) << "Parameters: "
           << " [connect_timeout]: " << params.connect_timeout_in_seconds
           << " [ping_interval]: " << params.ping_interval_in_seconds
           << " [liveness_timeout]: " << params.liveness_timeout_in_seconds
           << " [dynamic_timeout_delta]: "
           << params.dynamic_timeout_delta_in_seconds;

  if (!params.Validate())
    return OpenParams();

  return params;
}

}  // namespace media_router
