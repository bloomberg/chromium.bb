// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_factory_impl.h"

#include <string>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gcm/engine/connection_handler_impl.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/proxy_fallback.h"
#include "net/log/net_log_source_type.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/ssl/ssl_config_service.h"
#include "services/network/proxy_resolving_client_socket.h"

namespace gcm {

namespace {

// The amount of time a Socket read should wait before timing out.
const int kReadTimeoutMs = 30000;  // 30 seconds.

// If a connection is reset after succeeding within this window of time,
// the previous backoff entry is restored (and the connection success is treated
// as if it was transient).
const int kConnectionResetWindowSecs = 10;  // 10 seconds.

// Decides whether the last login was within kConnectionResetWindowSecs of now
// or not.
bool ShouldRestorePreviousBackoff(const base::TimeTicks& login_time,
                                  const base::TimeTicks& now_ticks) {
  return !login_time.is_null() &&
      now_ticks - login_time <=
          base::TimeDelta::FromSeconds(kConnectionResetWindowSecs);
}

}  // namespace

ConnectionFactoryImpl::ConnectionFactoryImpl(
    const std::vector<GURL>& mcs_endpoints,
    const net::BackoffEntry::Policy& backoff_policy,
    net::HttpNetworkSession* gcm_network_session,
    net::HttpNetworkSession* http_network_session,
    net::NetLog* net_log,
    GCMStatsRecorder* recorder)
    : mcs_endpoints_(mcs_endpoints),
      next_endpoint_(0),
      last_successful_endpoint_(0),
      backoff_policy_(backoff_policy),
      gcm_network_session_(gcm_network_session),
      http_network_session_(http_network_session),
      connecting_(false),
      waiting_for_backoff_(false),
      waiting_for_network_online_(false),
      handshake_in_progress_(false),
      recorder_(recorder),
      listener_(NULL),
      weak_ptr_factory_(this) {
  DCHECK_GE(mcs_endpoints_.size(), 1U);
  DCHECK(!http_network_session_ ||
         (gcm_network_session_ != http_network_session_));
}

ConnectionFactoryImpl::~ConnectionFactoryImpl() {
  CloseSocket();
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void ConnectionFactoryImpl::Initialize(
    const BuildLoginRequestCallback& request_builder,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback) {
  DCHECK(!connection_handler_);
  DCHECK(read_callback_.is_null());
  DCHECK(write_callback_.is_null());

  previous_backoff_ = CreateBackoffEntry(&backoff_policy_);
  backoff_entry_ = CreateBackoffEntry(&backoff_policy_);
  request_builder_ = request_builder;
  read_callback_ = read_callback;
  write_callback_ = write_callback;

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  waiting_for_network_online_ = net::NetworkChangeNotifier::IsOffline();
}

ConnectionHandler* ConnectionFactoryImpl::GetConnectionHandler() const {
  return connection_handler_.get();
}

void ConnectionFactoryImpl::Connect() {
  if (!connection_handler_) {
    connection_handler_ = CreateConnectionHandler(
        base::TimeDelta::FromMilliseconds(kReadTimeoutMs), read_callback_,
        write_callback_,
        base::Bind(&ConnectionFactoryImpl::ConnectionHandlerCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (connecting_ || waiting_for_backoff_)
    return;  // Connection attempt already in progress or pending.

  if (IsEndpointReachable())
    return;  // Already connected.

  ConnectWithBackoff();
}

ConnectionEventTracker* ConnectionFactoryImpl::GetEventTrackerForTesting() {
  return &event_tracker_;
}

void ConnectionFactoryImpl::ConnectWithBackoff() {
  // If a canary managed to connect while a backoff expiration was pending,
  // just cleanup the internal state.
  if (connecting_ || handshake_in_progress_ || IsEndpointReachable()) {
    waiting_for_backoff_ = false;
    return;
  }

  if (backoff_entry_->ShouldRejectRequest()) {
    DVLOG(1) << "Delaying MCS endpoint connection for "
             << backoff_entry_->GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    waiting_for_backoff_ = true;
    recorder_->RecordConnectionDelayedDueToBackoff(
        backoff_entry_->GetTimeUntilRelease().InMilliseconds());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ConnectionFactoryImpl::ConnectWithBackoff,
                   weak_ptr_factory_.GetWeakPtr()),
        backoff_entry_->GetTimeUntilRelease());
    return;
  }

  DVLOG(1) << "Attempting connection to MCS endpoint.";
  waiting_for_backoff_ = false;
  // It's necessary to close the socket before attempting any new connection,
  // otherwise it's possible to hit a use-after-free in the connection handler.
  // crbug.com/462319
  CloseSocket();
  ConnectImpl();
}

bool ConnectionFactoryImpl::IsEndpointReachable() const {
  return connection_handler_ && connection_handler_->CanSendMessage();
}

std::string ConnectionFactoryImpl::GetConnectionStateString() const {
  if (IsEndpointReachable())
    return "CONNECTED";
  if (handshake_in_progress_)
    return "HANDSHAKE IN PROGRESS";
  if (connecting_)
    return "CONNECTING";
  if (waiting_for_backoff_)
    return "WAITING FOR BACKOFF";
  if (waiting_for_network_online_)
    return "WAITING FOR NETWORK CHANGE";
  return "NOT CONNECTED";
}

void ConnectionFactoryImpl::SignalConnectionReset(
    ConnectionResetReason reason) {
  if (!connection_handler_) {
    // No initial connection has been made. No need to do anything.
    return;
  }

  // A failure can trigger multiple resets, so no need to do anything if a
  // connection is already in progress.
  if (connecting_) {
    DVLOG(1) << "Connection in progress, ignoring reset.";
    return;
  }

  if (listener_)
    listener_->OnDisconnected();

  UMA_HISTOGRAM_ENUMERATION("GCM.ConnectionResetReason",
                            reason,
                            CONNECTION_RESET_COUNT);
  recorder_->RecordConnectionResetSignaled(reason);
  if (!last_login_time_.is_null()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("GCM.ConnectionUpTime",
                               NowTicks() - last_login_time_,
                               base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromHours(24),
                               50);
    // |last_login_time_| will be reset below, before attempting the new
    // connection.
  }

  // SignalConnectionReset can be called at any time without regard to whether
  // a connection attempt is currently in progress. Only notify the event
  // tracker if there is an event in progress.
  if (event_tracker_.IsEventInProgress()) {
    if (reason == LOGIN_FAILURE)
      event_tracker_.ConnectionLoginFailed();
    event_tracker_.EndConnectionAttempt();
  }

  CloseSocket();
  DCHECK(!IsEndpointReachable());

  // TODO(zea): if the network is offline, don't attempt to connect.
  // See crbug.com/396687

  // Network changes get special treatment as they can trigger a one-off canary
  // request that bypasses backoff (but does nothing if a connection is in
  // progress). Other connection reset events can be ignored as a connection
  // is already awaiting backoff expiration.
  if (waiting_for_backoff_ && reason != NETWORK_CHANGE) {
    DVLOG(1) << "Backoff expiration pending, ignoring reset.";
    return;
  }

  if (reason == NETWORK_CHANGE) {
    // Canary attempts bypass backoff without resetting it. These will have no
    // effect if we're already in the process of connecting.
    ConnectImpl();
    return;
  } else if (handshake_in_progress_) {
    // Failures prior to handshake completion reuse the existing backoff entry.
    handshake_in_progress_ = false;
    backoff_entry_->InformOfRequest(false);
  } else if (reason == LOGIN_FAILURE ||
             ShouldRestorePreviousBackoff(last_login_time_, NowTicks())) {
    // Failures due to login, or within the reset window of a login, restore
    // the backoff entry that was saved off at login completion time.
    backoff_entry_.swap(previous_backoff_);
    backoff_entry_->InformOfRequest(false);
  } else {
    // We shouldn't be in backoff in thise case.
    DCHECK_EQ(0, backoff_entry_->failure_count());
  }

  // At this point the last login time has been consumed or deemed irrelevant,
  // reset it.
  last_login_time_ = base::TimeTicks();

  Connect();
}

void ConnectionFactoryImpl::SetConnectionListener(
    ConnectionListener* listener) {
  listener_ = listener;
}

base::TimeTicks ConnectionFactoryImpl::NextRetryAttempt() const {
  if (!backoff_entry_)
    return base::TimeTicks();
  return backoff_entry_->GetReleaseTime();
}

void ConnectionFactoryImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    DVLOG(1) << "Network lost, resettion connection.";
    waiting_for_network_online_ = true;

    // Will do nothing due to |waiting_for_network_online_ == true|.
    // TODO(zea): make the above statement actually true. See crbug.com/396687
    SignalConnectionReset(NETWORK_CHANGE);
    return;
  }

  DVLOG(1) << "Connection type changed to " << type << ", reconnecting.";
  waiting_for_network_online_ = false;
  SignalConnectionReset(NETWORK_CHANGE);
}

GURL ConnectionFactoryImpl::GetCurrentEndpoint() const {
  // Note that IsEndpointReachable() returns false anytime connecting_ is true,
  // so while connecting this always uses |next_endpoint_|.
  if (IsEndpointReachable())
    return mcs_endpoints_[last_successful_endpoint_];
  return mcs_endpoints_[next_endpoint_];
}

net::IPEndPoint ConnectionFactoryImpl::GetPeerIP() {
  if (!socket_)
    return net::IPEndPoint();

  net::IPEndPoint ip_endpoint;
  int result = socket_->GetPeerAddress(&ip_endpoint);
  if (result != net::OK)
    return net::IPEndPoint();

  return ip_endpoint;
}

void ConnectionFactoryImpl::ConnectImpl() {
  event_tracker_.StartConnectionAttempt();
  StartConnection();
}

void ConnectionFactoryImpl::StartConnection() {
  DCHECK(!IsEndpointReachable());
  // TODO(zea): Make this a dcheck again. crbug.com/462319
  CHECK(!socket_);

  // TODO(zea): if the network is offline, don't attempt to connect.
  // See crbug.com/396687

  connecting_ = true;
  GURL current_endpoint = GetCurrentEndpoint();
  recorder_->RecordConnectionInitiated(current_endpoint.host());
  UpdateFromHttpNetworkSession();
  net::SSLConfig ssl_config;
  gcm_network_session_->ssl_config_service()->GetSSLConfig(&ssl_config);
  socket_ = std::make_unique<network::ProxyResolvingClientSocket>(
      gcm_network_session_, ssl_config, current_endpoint, true /*use_tls*/);
  int status = socket_->Connect(base::BindRepeating(
      &ConnectionFactoryImpl::OnConnectDone, weak_ptr_factory_.GetWeakPtr()));
  if (status != net::ERR_IO_PENDING)
    OnConnectDone(status);
}

void ConnectionFactoryImpl::InitHandler() {
  // May be null in tests.
  mcs_proto::LoginRequest login_request;
  if (!request_builder_.is_null()) {
    request_builder_.Run(&login_request);
    DCHECK(login_request.IsInitialized());
    event_tracker_.WriteToLoginRequest(&login_request);
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcm_connection_factory", R"(
        semantics {
          sender: "GCM Connection Factory"
          description:
            "TCP connection to the Google Cloud Messaging notification "
            "servers. Supports reliable bi-directional messaging and push "
            "notifications for multiple consumers."
          trigger:
            "The connection is created when an application (e.g. Chrome Sync) "
            "or a website using Web Push starts the GCM service, and is kept "
            "alive as long as there are valid applications registered. "
            "Messaging is application/website controlled."
          data:
            "Arbitrary application-specific data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can stop messages related to Sync by disabling Sync for "
            "everything in settings. Messages related to Web Push can be "
            "stopped by revoking the site permissions in settings. Messages "
            "related to extensions can be stopped by uninstalling the "
            "extension."
          chrome_policy {
            SyncDisabled {
              SyncDisabled: True
            }
          }
        }
        comments:
          "'SyncDisabled' policy disables messages that are based on Sync, "
          "but does not have any effect on other Google Cloud messages."
        )");

  connection_handler_->Init(login_request, traffic_annotation, socket_.get());
}

std::unique_ptr<net::BackoffEntry> ConnectionFactoryImpl::CreateBackoffEntry(
    const net::BackoffEntry::Policy* const policy) {
  return std::unique_ptr<net::BackoffEntry>(new net::BackoffEntry(policy));
}

std::unique_ptr<ConnectionHandler>
ConnectionFactoryImpl::CreateConnectionHandler(
    base::TimeDelta read_timeout,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback,
    const ConnectionHandler::ConnectionChangedCallback& connection_callback) {
  return base::WrapUnique<ConnectionHandler>(new ConnectionHandlerImpl(
      read_timeout, read_callback, write_callback, connection_callback));
}

base::TimeTicks ConnectionFactoryImpl::NowTicks() {
  return base::TimeTicks::Now();
}

void ConnectionFactoryImpl::OnConnectDone(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result != net::OK) {
    LOG(ERROR) << "Failed to connect to MCS endpoint with error " << result;
    UMA_HISTOGRAM_BOOLEAN("GCM.ConnectionSuccessRate", false);
    recorder_->RecordConnectionFailure(result);
    CloseSocket();
    backoff_entry_->InformOfRequest(false);
    base::UmaHistogramSparse("GCM.ConnectionFailureErrorCode", result);

    event_tracker_.ConnectionAttemptFailed(result);
    event_tracker_.EndConnectionAttempt();

    // If there are other endpoints available, use the next endpoint on the
    // subsequent retry.
    next_endpoint_++;
    if (next_endpoint_ >= mcs_endpoints_.size())
      next_endpoint_ = 0;
    connecting_ = false;
    Connect();
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("GCM.ConnectionSuccessRate", true);
  UMA_HISTOGRAM_COUNTS("GCM.ConnectionEndpoint", next_endpoint_);
  recorder_->RecordConnectionSuccess();

  // Reset the endpoint back to the default.
  // TODO(zea): consider prioritizing endpoints more intelligently based on
  // which ones succeed most for this client? Although that will affect
  // measuring the success rate of the default endpoint vs fallback.
  last_successful_endpoint_ = next_endpoint_;
  next_endpoint_ = 0;
  connecting_ = false;
  handshake_in_progress_ = true;
  DVLOG(1) << "MCS endpoint socket connection success, starting login.";
  InitHandler();
}

void ConnectionFactoryImpl::ConnectionHandlerCallback(int result) {
  DCHECK(!connecting_);
  if (result != net::OK) {
    // TODO(zea): Consider how to handle errors that may require some sort of
    // user intervention (login page, etc.).
    base::UmaHistogramSparse("GCM.ConnectionDisconnectErrorCode", result);
    SignalConnectionReset(SOCKET_FAILURE);
    return;
  }

  // Handshake complete, reset backoff. If the login failed with an error,
  // the client should invoke SignalConnectionReset(LOGIN_FAILURE), which will
  // restore the previous backoff.
  DVLOG(1) << "Handshake complete.";
  last_login_time_ = NowTicks();
  previous_backoff_.swap(backoff_entry_);
  backoff_entry_->Reset();
  handshake_in_progress_ = false;

  event_tracker_.ConnectionAttemptSucceeded();

  if (listener_)
    listener_->OnConnected(GetCurrentEndpoint(), GetPeerIP());
}

void ConnectionFactoryImpl::CloseSocket() {
  // The connection handler needs to be reset, else it'll attempt to keep using
  // the destroyed socket.
  if (connection_handler_)
    connection_handler_->Reset();

  if (socket_)
    socket_->Disconnect();
  socket_ = nullptr;
}

void ConnectionFactoryImpl::UpdateFromHttpNetworkSession() {
  if (!http_network_session_ || !http_network_session_->http_auth_cache())
    return;

  gcm_network_session_->http_auth_cache()->UpdateAllFrom(
      *http_network_session_->http_auth_cache());

  if (!http_network_session_->IsQuicEnabled())
    gcm_network_session_->DisableQuic();
}

}  // namespace gcm
