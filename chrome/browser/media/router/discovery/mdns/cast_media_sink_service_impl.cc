// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/media_sink.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/logger.h"
#include "components/net_log/chrome_net_log.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_errors.h"

namespace media_router {

namespace {

MediaSinkInternal CreateCastSinkFromDialSink(
    const MediaSinkInternal& dial_sink) {
  const std::string& friendly_name = dial_sink.sink().name();

  // Replace the "dial:" prefix with "cast:".
  std::string sink_id =
      CastMediaSinkServiceImpl::GetCastSinkIdFromDial(dial_sink.sink().id());

  // Note that the real sink icon will be determined later using information
  // from the opened cast channel.
  MediaSink sink(sink_id, friendly_name, SinkIconType::CAST,
                 MediaRouteProviderId::CAST);

  CastSinkExtraData extra_data;
  extra_data.ip_endpoint =
      net::IPEndPoint(dial_sink.dial_data().ip_address, kCastControlPort);
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
      net::IsCertificateError(last_error.net_return_value) ||
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

// Max allowed values
constexpr int kMaxConnectTimeoutInSeconds = 30;
constexpr int kMaxLivenessTimeoutInSeconds = 60;

// Max failure count allowed for a Cast channel.
constexpr int kMaxFailureCount = 100;

bool IsNetworkIdUnknownOrDisconnected(const std::string& network_id) {
  return network_id == DiscoveryNetworkMonitor::kNetworkIdUnknown ||
         network_id == DiscoveryNetworkMonitor::kNetworkIdDisconnected;
}

// Updates |existing_sink| with properties from |new_sink|. The relevant
// properties are sink name and capabilities (and icon type, by association).
// This method is only called with a |new_sink| discovered by mDNS. As such,
// |discovered_by_dial| is also updated to false.
void UpdateCastSink(const MediaSinkInternal& new_sink,
                    MediaSinkInternal* existing_sink) {
  existing_sink->sink().set_name(new_sink.sink().name());
  auto capabilities = new_sink.cast_data().capabilities;
  existing_sink->cast_data().capabilities = capabilities;
  existing_sink->sink().set_icon_type(GetCastSinkIconType(capabilities));
  existing_sink->cast_data().discovered_by_dial = false;
}

}  // namespace

// static
constexpr int CastMediaSinkServiceImpl::kMaxDialSinkFailureCount;

// static
MediaSink::Id CastMediaSinkServiceImpl::GetCastSinkIdFromDial(
    const MediaSink::Id& dial_sink_id) {
  DCHECK_EQ("dial:", dial_sink_id.substr(0, 5))
      << "unexpected DIAL sink id " << dial_sink_id;

  // Replace the "dial:" prefix with "cast:".
  return "cast:" + dial_sink_id.substr(5);
}

// static
MediaSink::Id CastMediaSinkServiceImpl::GetDialSinkIdFromCast(
    const MediaSink::Id& cast_sink_id) {
  DCHECK_EQ("cast:", cast_sink_id.substr(0, 5))
      << "unexpected Cast sink id " << cast_sink_id;

  // Replace the "cast:" prefix with "dial:".
  return "dial:" + cast_sink_id.substr(5);
}

CastMediaSinkServiceImpl::CastMediaSinkServiceImpl(
    const OnSinksDiscoveredCallback& callback,
    cast_channel::CastSocketService* cast_socket_service,
    DiscoveryNetworkMonitor* network_monitor,
    MediaSinkServiceBase* dial_media_sink_service,
    bool allow_all_ips)
    : MediaSinkServiceBase(callback),
      cast_socket_service_(cast_socket_service),
      network_monitor_(network_monitor),
      allow_all_ips_(allow_all_ips),
      dial_media_sink_service_(dial_media_sink_service),
      task_runner_(cast_socket_service_->task_runner()),
      clock_(base::DefaultClock::GetInstance()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(cast_socket_service_);
  DCHECK(network_monitor_);

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
  dial_media_sink_service_->RemoveObserver(this);
  network_monitor_->RemoveObserver(this);
  cast_socket_service_->RemoveObserver(this);
}

void CastMediaSinkServiceImpl::SetClockForTest(base::Clock* clock) {
  clock_ = clock;
}

void CastMediaSinkServiceImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cast_socket_service_->AddObserver(this);

  // This call to |GetNetworkId| ensures that we get the current network ID at
  // least once during startup in case |AddObserver| occurs after the first
  // round of notifications has already been dispatched.
  network_monitor_->GetNetworkId(base::BindOnce(
      &CastMediaSinkServiceImpl::OnNetworksChanged, GetWeakPtr()));
  network_monitor_->AddObserver(this);

  dial_media_sink_service_->AddObserver(this);

  std::vector<MediaSinkInternal> test_sinks = GetFixedIPSinksFromCommandLine();
  if (!test_sinks.empty())
    OpenChannels(test_sinks, SinkSource::kMdns);
}

void CastMediaSinkServiceImpl::RecordDeviceCounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_.RecordDeviceCountsIfNeeded(GetSinks().size(),
                                      known_ip_endpoints_.size());
}

void CastMediaSinkServiceImpl::OnUserGesture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Re-sync sinks from DialMediaSinkService. It's possible that a
  // DIAL-discovered sink was added here earlier, but was removed due to flaky
  // network. This gives CastMediaSinkServiceImpl an opportunity to recover even
  // if mDNS is not working for some reason.
  for (const auto& sink : dial_media_sink_service_->GetSinks())
    TryConnectDialDiscoveredSink(sink.second);
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
    known_ip_endpoints_.insert(cast_sink.cast_data().ip_endpoint);
    OpenChannel(cast_sink, nullptr, sink_source);
  }

  StartTimer();
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
  int socket_id = socket.id();
  task_runner_->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&cast_channel::CastSocketService::RemoveSocket),
          base::Unretained(cast_socket_service_), socket_id));

  // Remove existing cast sink from |sinks|. It will be added back if
  // it can be successfully reconnected.
  const auto& sinks = GetSinks();
  auto sink_it =
      std::find_if(sinks.begin(), sinks.end(), [&socket_id](const auto& entry) {
        return entry.second.cast_data().cast_channel_id == socket_id;
      });
  if (sink_it == sinks.end()) {
    DVLOG(2) << "Cannot find existing cast sink. Skip reopen cast channel: "
             << ip_endpoint.ToString();
    return;
  }

  // We erase the sink here so that OpenChannel would not find an existing
  // sink.
  // Note: a better longer term solution is to introduce a state field to the
  // sink. We would set it to ERROR here. In OpenChannel(), we would check
  // create a socket only if the state is not already CONNECTED.
  MediaSinkInternal sink = sink_it->second;
  RemoveSink(sink);

  // If socket is not opened yet, then |OnChannelOpened()| will handle the
  // retry.
  if (socket.ready_state() != cast_channel::ReadyState::CONNECTING) {
    DVLOG(2) << "OnError starts reopening cast channel: "
             << ip_endpoint.ToString();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel, GetWeakPtr(),
                       sink, nullptr, SinkSource::kConnectionRetry));
  }
}

void CastMediaSinkServiceImpl::OnMessage(
    const cast_channel::CastSocket& socket,
    const cast::channel::CastMessage& message) {}

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
    std::vector<MediaSinkInternal> current_sinks;
    for (const auto& entry : GetSinks())
      current_sinks.push_back(entry.second);

    sink_cache_[last_network_id] = std::move(current_sinks);
  }

  // TODO(imcheng): Maybe this should clear |sinks_| and call |StartTimer()|
  // so it is more responsive?
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
    const MediaSinkInternal& sink) {
  int connect_timeout_in_seconds = open_params_.connect_timeout_in_seconds;
  int liveness_timeout_in_seconds = open_params_.liveness_timeout_in_seconds;
  int delta_in_seconds = open_params_.dynamic_timeout_delta_in_seconds;

  auto it = failure_count_map_.find(sink.sink().id());
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
      sink.cast_data().ip_endpoint,
      base::TimeDelta::FromSeconds(connect_timeout_in_seconds),
      base::TimeDelta::FromSeconds(liveness_timeout_in_seconds),
      base::TimeDelta::FromSeconds(open_params_.ping_interval_in_seconds),
      cast_channel::CastDeviceCapability::NONE);
}

void CastMediaSinkServiceImpl::OpenChannel(
    const MediaSinkInternal& cast_sink,
    std::unique_ptr<net::BackoffEntry> backoff_entry,
    SinkSource sink_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
  if (!allow_all_ips_ && ip_endpoint.address().IsPubliclyRoutable()) {
    DVLOG(2) << "Invalid Cast IP address: " << ip_endpoint.address().ToString();
    return;
  }

  // Erase the entry from |dial_sink_failure_count_| since the device is now
  // known to be a Cast device.
  const MediaSink::Id& sink_id = cast_sink.sink().id();
  if (sink_source != SinkSource::kDial)
    dial_sink_failure_count_.erase(sink_id);

  // If the sink already exists, then we need to check if there are updates.
  // If the IP endpoint changed, then we will need to reopen a socket.
  // If the IP endpoint remained the same but other properties changed, then we
  // can update the existing sink without opening a new socket.
  const MediaSinkInternal* existing_sink = GetSinkById(sink_id);
  if (existing_sink && existing_sink->cast_data().ip_endpoint == ip_endpoint) {
    DVLOG(2) << "A channel already exists for " << sink_id << ", "
             << ip_endpoint.ToString();
    // This update is only performed if |sink_source| is kMdns. In particular,
    // DIAL-discovered
    // sinks contain incomplete information which should not be used for
    // updates.
    if (sink_source != SinkSource::kMdns)
      return;

    if (existing_sink->sink().name() == cast_sink.sink().name() &&
        existing_sink->cast_data().capabilities ==
            cast_sink.cast_data().capabilities) {
      return;
    }

    // Merge new fields into copy of existing sink to retain cast_channel_id.
    DVLOG(2) << "Updating existing sink without opening new channel: "
             << sink_id << ", name: " << cast_sink.sink().name();
    MediaSinkInternal existing_sink_copy = *existing_sink;
    UpdateCastSink(cast_sink, &existing_sink_copy);
    AddOrUpdateSink(existing_sink_copy);
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
      CreateCastSocketOpenParams(cast_sink);
  cast_socket_service_->OpenSocket(
      base::BindRepeating([] {
        DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
        return g_browser_process->system_network_context_manager()
            ->GetContext();
      }),
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

  const MediaSink::Id& sink_id = cast_sink.sink().id();
  const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
  if (sink_source == SinkSource::kDial)
    ++dial_sink_failure_count_[sink_id];

  int failure_count = ++failure_count_map_[sink_id];
  failure_count_map_[sink_id] = std::min(failure_count, kMaxFailureCount);

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
                     cast_sink, std::move(backoff_entry), sink_source),
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
  CastSinkExtraData& extra_data = cast_sink.cast_data();
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

  // Add or update existing cast sink.
  DVLOG(2) << "Adding or updating sink [name]: " << cast_sink.sink().name();
  const MediaSink::Id& sink_id = cast_sink.sink().id();
  const MediaSinkInternal* existing_sink = GetSinkById(sink_id);
  if (!existing_sink) {
    metrics_.RecordCastSinkDiscoverySource(sink_source);
  } else {
    if (existing_sink->cast_data().discovered_by_dial &&
        !cast_sink.cast_data().discovered_by_dial) {
      metrics_.RecordCastSinkDiscoverySource(SinkSource::kDialMdns);
    }
  }
  AddOrUpdateSink(cast_sink);
  failure_count_map_.erase(sink_id);

  // To maintain the invariant that an IPEndpoint appears in at most one entry
  // in the sink list, we will remove the sink (if any) that has the same
  // IPEndPoint but different sink ID.
  const net::IPEndPoint& ip_endpoint = extra_data.ip_endpoint;
  const auto& sinks = GetSinks();
  auto old_sink_it =
      std::find_if(sinks.begin(), sinks.end(),
                   [&cast_sink, &ip_endpoint](const auto& entry) {
                     return entry.first != cast_sink.sink().id() &&
                            entry.second.cast_data().ip_endpoint == ip_endpoint;
                   });

  if (old_sink_it != sinks.end())
    RemoveSink(old_sink_it->second);

  // Certain classes of Cast sinks support advertising via SSDP but do not
  // properly implement the rest of the DIAL protocol. If we successfully open
  // a Cast channel to a device that came from DIAL, remove it from
  // |dial_media_sink_service_|. This ensures the device shows up as a Cast sink
  // only.
  dial_media_sink_service_->RemoveSinkById(GetDialSinkIdFromCast(sink_id));
}

void CastMediaSinkServiceImpl::OnChannelOpenFailed(
    const net::IPEndPoint& ip_endpoint,
    const MediaSinkInternal& sink) {
  // Check that the IPEndPoints match before removing, as it is possible that
  // the sink was reconnected under a different IP before this method is called.
  const MediaSinkInternal* existing_sink = GetSinkById(sink.sink().id());
  if (!existing_sink ||
      !(ip_endpoint == existing_sink->cast_data().ip_endpoint))
    return;

  RemoveSink(sink);
}

void CastMediaSinkServiceImpl::OnSinkAddedOrUpdated(
    const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TryConnectDialDiscoveredSink(sink);
}

void CastMediaSinkServiceImpl::OnSinkRemoved(const MediaSinkInternal& sink) {
  // No-op
}

void CastMediaSinkServiceImpl::TryConnectDialDiscoveredSink(
    const MediaSinkInternal& dial_sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/753175): Dual discovery should not try to open cast channel
  // for non-Cast device.
  if (IsProbablyNonCastDevice(dial_sink)) {
    DVLOG(2) << "Skip open channel for DIAL-discovered device because it "
             << "is probably not a Cast device: " << dial_sink.sink().name();
    return;
  }

  MediaSinkInternal sink = CreateCastSinkFromDialSink(dial_sink);
  if (GetSinkById(sink.sink().id())) {
    DVLOG(2) << "Sink discovered by mDNS, skip adding [name]: "
             << sink.sink().name();
    metrics_.RecordCastSinkDiscoverySource(SinkSource::kMdnsDial);
    // Sink is a Cast device; remove from |dial_media_sink_service_| to prevent
    // duplicates.
    dial_media_sink_service_->RemoveSink(dial_sink);
    return;
  }

  OpenChannel(sink, nullptr, SinkSource::kDial);
}

bool CastMediaSinkServiceImpl::IsProbablyNonCastDevice(
    const MediaSinkInternal& dial_sink) const {
  auto it = dial_sink_failure_count_.find(
      GetCastSinkIdFromDial(dial_sink.sink().id()));
  return it != dial_sink_failure_count_.end() &&
         it->second >= kMaxDialSinkFailureCount;
}

void CastMediaSinkServiceImpl::OpenChannelsNow(
    const std::vector<MediaSinkInternal>& cast_sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OpenChannels(cast_sinks, SinkSource::kConnectionRetry);
}

void CastMediaSinkServiceImpl::SetCastAllowAllIPs(bool allow_all_ips) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  allow_all_ips_ = allow_all_ips;
}

}  // namespace media_router
