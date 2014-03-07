// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "net/http/http_status_code.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using captive_portal::CaptivePortalDetector;

namespace chromeos {

namespace {

// Delay before portal detection caused by changes in proxy settings.
const int kProxyChangeDelaySec = 1;

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

void RecordDiscrepancyWithShill(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  if (network->connection_state() == shill::kStateOnline) {
    UMA_HISTOGRAM_ENUMERATION(
        NetworkPortalDetectorImpl::kShillOnlineHistogram,
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  } else if (network->connection_state() == shill::kStatePortal) {
    UMA_HISTOGRAM_ENUMERATION(
        NetworkPortalDetectorImpl::kShillPortalHistogram,
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  } else if (network->connection_state() == shill::kStateOffline) {
    UMA_HISTOGRAM_ENUMERATION(
        NetworkPortalDetectorImpl::kShillOfflineHistogram,
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, public:

const char NetworkPortalDetectorImpl::kDetectionResultHistogram[] =
    "CaptivePortal.OOBE.DetectionResult";
const char NetworkPortalDetectorImpl::kDetectionDurationHistogram[] =
    "CaptivePortal.OOBE.DetectionDuration";
const char NetworkPortalDetectorImpl::kShillOnlineHistogram[] =
    "CaptivePortal.OOBE.DiscrepancyWithShill_Online";
const char NetworkPortalDetectorImpl::kShillPortalHistogram[] =
    "CaptivePortal.OOBE.DiscrepancyWithShill_RestrictedPool";
const char NetworkPortalDetectorImpl::kShillOfflineHistogram[] =
    "CaptivePortal.OOBE.DiscrepancyWithShill_Offline";

NetworkPortalDetectorImpl::NetworkPortalDetectorImpl(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : state_(STATE_IDLE),
      test_url_(CaptivePortalDetector::kDefaultURL),
      enabled_(false),
      weak_factory_(this),
      attempt_count_(0),
      strategy_(PortalDetectorStrategy::CreateById(
          PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN)) {
  captive_portal_detector_.reset(new CaptivePortalDetector(request_context));
  strategy_->set_delegate(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_PROXY_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());

  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

NetworkPortalDetectorImpl::~NetworkPortalDetectorImpl() {
  DCHECK(CalledOnValidThread());

  attempt_task_.Cancel();
  attempt_timeout_.Cancel();

  captive_portal_detector_->Cancel();
  captive_portal_detector_.reset();
  observers_.Clear();
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void NetworkPortalDetectorImpl::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  if (observer && !observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkPortalDetectorImpl::AddAndFireObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  if (!observer)
    return;
  AddObserver(observer);
  CaptivePortalState portal_state;
  const NetworkState* network = DefaultNetwork();
  if (network)
    portal_state = GetCaptivePortalState(network->path());
  observer->OnPortalDetectionCompleted(network, portal_state);
}

void NetworkPortalDetectorImpl::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  if (observer)
    observers_.RemoveObserver(observer);
}

bool NetworkPortalDetectorImpl::IsEnabled() { return enabled_; }

void NetworkPortalDetectorImpl::Enable(bool start_detection) {
  DCHECK(CalledOnValidThread());
  if (enabled_)
    return;

  DCHECK(is_idle());
  enabled_ = true;

  const NetworkState* network = DefaultNetwork();
  if (!start_detection || !network)
    return;
  portal_state_map_.erase(network->path());
  StartDetection();
}

NetworkPortalDetectorImpl::CaptivePortalState
NetworkPortalDetectorImpl::GetCaptivePortalState(
    const std::string& service_path) {
  DCHECK(CalledOnValidThread());
  CaptivePortalStateMap::const_iterator it =
      portal_state_map_.find(service_path);
  if (it == portal_state_map_.end())
    return CaptivePortalState();
  return it->second;
}

bool NetworkPortalDetectorImpl::StartDetectionIfIdle() {
  if (!is_idle())
    return false;
  StartDetection();
  return true;
}

void NetworkPortalDetectorImpl::EnableErrorScreenStrategy() {
  if (strategy_->Id() == PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN)
    return;
  VLOG(1) << "Error screen detection strategy enabled.";
  strategy_ = PortalDetectorStrategy::CreateById(
      PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN);
  strategy_->set_delegate(this);
  StartDetectionIfIdle();
}

void NetworkPortalDetectorImpl::DisableErrorScreenStrategy() {
  if (strategy_->Id() != PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN)
    return;
  VLOG(1) << "Error screen detection strategy disabled.";
  strategy_ = PortalDetectorStrategy::CreateById(
      PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN);
  strategy_->set_delegate(this);
  StopDetection();
}

void NetworkPortalDetectorImpl::DefaultNetworkChanged(
    const NetworkState* default_network) {
  DCHECK(CalledOnValidThread());

  if (!default_network) {
    default_network_name_.clear();
    default_network_id_.clear();

    StopDetection();

    CaptivePortalState state;
    state.status = CAPTIVE_PORTAL_STATUS_OFFLINE;
    OnDetectionCompleted(NULL, state);
    return;
  }

  default_network_name_ = default_network->name();
  default_network_id_ = default_network->guid();

  bool network_changed = (default_service_path_ != default_network->path());
  default_service_path_ = default_network->path();

  bool connection_state_changed =
      (default_connection_state_ != default_network->connection_state());
  default_connection_state_ = default_network->connection_state();

  if (network_changed || connection_state_changed)
    StopDetection();

  if (CanPerformAttempt() &&
      NetworkState::StateIsConnected(default_connection_state_)) {
    // Initiate Captive Portal detection if network's captive
    // portal state is unknown (e.g. for freshly created networks),
    // offline or if network connection state was changed.
    CaptivePortalState state = GetCaptivePortalState(default_network->path());
    if (state.status == CAPTIVE_PORTAL_STATUS_UNKNOWN ||
        state.status == CAPTIVE_PORTAL_STATUS_OFFLINE ||
        (!network_changed && connection_state_changed)) {
      ScheduleAttempt(base::TimeDelta());
    }
  }
}

int NetworkPortalDetectorImpl::AttemptCount() { return attempt_count_; }

base::TimeTicks NetworkPortalDetectorImpl::AttemptStartTime() {
  return attempt_start_time_;
}

base::TimeTicks NetworkPortalDetectorImpl::GetCurrentTimeTicks() {
  if (time_ticks_for_testing_.is_null())
    return base::TimeTicks::Now();
  return time_ticks_for_testing_;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, private:

void NetworkPortalDetectorImpl::StartDetection() {
  attempt_count_ = 0;
  DCHECK(CanPerformAttempt());
  detection_start_time_ = GetCurrentTimeTicks();
  ScheduleAttempt(base::TimeDelta());
}

void NetworkPortalDetectorImpl::StopDetection() {
  attempt_task_.Cancel();
  attempt_timeout_.Cancel();
  captive_portal_detector_->Cancel();
  state_ = STATE_IDLE;
  attempt_count_ = 0;
}

bool NetworkPortalDetectorImpl::CanPerformAttempt() const {
  return is_idle() && strategy_->CanPerformAttempt();
}

void NetworkPortalDetectorImpl::ScheduleAttempt(const base::TimeDelta& delay) {
  DCHECK(CanPerformAttempt());

  if (!IsEnabled())
    return;

  attempt_task_.Cancel();
  attempt_timeout_.Cancel();
  state_ = STATE_PORTAL_CHECK_PENDING;

  next_attempt_delay_ = std::max(delay, strategy_->GetDelayTillNextAttempt());
  attempt_task_.Reset(base::Bind(&NetworkPortalDetectorImpl::StartAttempt,
                                 weak_factory_.GetWeakPtr()));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, attempt_task_.callback(), next_attempt_delay_);
}

void NetworkPortalDetectorImpl::StartAttempt() {
  DCHECK(is_portal_check_pending());

  state_ = STATE_CHECKING_FOR_PORTAL;
  attempt_start_time_ = GetCurrentTimeTicks();

  captive_portal_detector_->DetectCaptivePortal(
      test_url_,
      base::Bind(&NetworkPortalDetectorImpl::OnAttemptCompleted,
                 weak_factory_.GetWeakPtr()));
  attempt_timeout_.Reset(
      base::Bind(&NetworkPortalDetectorImpl::OnAttemptTimeout,
                 weak_factory_.GetWeakPtr()));

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      attempt_timeout_.callback(),
      strategy_->GetNextAttemptTimeout());
}

void NetworkPortalDetectorImpl::OnAttemptTimeout() {
  DCHECK(CalledOnValidThread());
  DCHECK(is_checking_for_portal());

  VLOG(1) << "Portal detection timeout: name=" << default_network_name_ << ", "
          << "id=" << default_network_id_;

  captive_portal_detector_->Cancel();
  CaptivePortalDetector::Results results;
  results.result = captive_portal::RESULT_NO_RESPONSE;
  OnAttemptCompleted(results);
}

void NetworkPortalDetectorImpl::OnAttemptCompleted(
    const CaptivePortalDetector::Results& results) {
  captive_portal::Result result = results.result;
  int response_code = results.response_code;

  DCHECK(CalledOnValidThread());
  DCHECK(is_checking_for_portal());

  VLOG(1) << "Detection attempt completed: "
          << "name=" << default_network_name_ << ", "
          << "id=" << default_network_id_ << ", "
          << "result="
          << CaptivePortalDetector::CaptivePortalResultToString(results.result)
          << ", "
          << "response_code=" << results.response_code;

  state_ = STATE_IDLE;
  attempt_timeout_.Cancel();
  ++attempt_count_;

  const NetworkState* network = DefaultNetwork();

  // If using a fake profile client, also fake being behind a captive portal
  // if the default network is in portal state.
  if (result != captive_portal::RESULT_NO_RESPONSE &&
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface() &&
      network && network->connection_state() == shill::kStatePortal) {
    result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;
    response_code = 200;
  }

  CaptivePortalState state;
  state.response_code = response_code;
  switch (result) {
    case captive_portal::RESULT_NO_RESPONSE:
      if (state.response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
        state.status = CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
      } else if (CanPerformAttempt()) {
        ScheduleAttempt(results.retry_after_delta);
        return;
      } else if (network &&
                 (network->connection_state() == shill::kStatePortal)) {
        // Take into account shill's detection results.
        state.status = CAPTIVE_PORTAL_STATUS_PORTAL;
        LOG(WARNING) << "Network name=" << network->name() << ", "
                     << "id=" << network->guid() << " "
                     << "is marked as "
                     << CaptivePortalStatusString(state.status) << " "
                     << "despite the fact that CaptivePortalDetector "
                     << "received no response";
      } else {
        state.status = CAPTIVE_PORTAL_STATUS_OFFLINE;
      }
      break;
    case captive_portal::RESULT_INTERNET_CONNECTED:
      state.status = CAPTIVE_PORTAL_STATUS_ONLINE;
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      state.status = CAPTIVE_PORTAL_STATUS_PORTAL;
      break;
    default:
      break;
  }

  OnDetectionCompleted(network, state);
  if (CanPerformAttempt() && strategy_->CanPerformAttemptAfterDetection())
    ScheduleAttempt(base::TimeDelta());
}

void NetworkPortalDetectorImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_LOGIN_PROXY_CHANGED ||
      type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
      type == chrome::NOTIFICATION_AUTH_CANCELLED) {
    VLOG(1) << "Restarting portal detection due to proxy change.";
    attempt_count_ = 0;
    if (is_portal_check_pending())
      return;
    StopDetection();
    ScheduleAttempt(base::TimeDelta::FromSeconds(kProxyChangeDelaySec));
  }
}

void NetworkPortalDetectorImpl::OnDetectionCompleted(
    const NetworkState* network,
    const CaptivePortalState& state) {
  if (!network) {
    NotifyDetectionCompleted(network, state);
    return;
  }

  CaptivePortalStateMap::const_iterator it =
      portal_state_map_.find(network->path());
  if (it == portal_state_map_.end() || it->second.status != state.status ||
      it->second.response_code != state.response_code) {
    VLOG(1) << "Updating Chrome Captive Portal state: "
            << "name=" << network->name() << ", "
            << "id=" << network->guid() << ", "
            << "status=" << CaptivePortalStatusString(state.status) << ", "
            << "response_code=" << state.response_code;

    // Record detection duration iff detection result differs from the
    // previous one for this network. The reason is to record all stats
    // only when network changes it's state.
    RecordDetectionStats(network, state.status);

    portal_state_map_[network->path()] = state;
  }
  NotifyDetectionCompleted(network, state);
}

void NetworkPortalDetectorImpl::NotifyDetectionCompleted(
    const NetworkState* network,
    const CaptivePortalState& state) {
  FOR_EACH_OBSERVER(
      Observer, observers_, OnPortalDetectionCompleted(network, state));
  notification_controller_.OnPortalDetectionCompleted(network, state);
}

bool NetworkPortalDetectorImpl::AttemptTimeoutIsCancelledForTesting() const {
  return attempt_timeout_.IsCancelled();
}

void NetworkPortalDetectorImpl::RecordDetectionStats(
    const NetworkState* network,
    CaptivePortalStatus status) {
  // Don't record stats for offline state.
  if (!network)
    return;

  if (!detection_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDetectionDurationHistogram,
                               GetCurrentTimeTicks() - detection_start_time_);
  }
  UMA_HISTOGRAM_ENUMERATION(kDetectionResultHistogram,
                            status,
                            NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
      NOTREACHED();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      if (network->connection_state() == shill::kStateOnline ||
          network->connection_state() == shill::kStatePortal) {
        RecordDiscrepancyWithShill(network, status);
      }
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      if (network->connection_state() != shill::kStateOnline)
        RecordDiscrepancyWithShill(network, status);
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      if (network->connection_state() != shill::kStatePortal)
        RecordDiscrepancyWithShill(network, status);
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      if (network->connection_state() != shill::kStateOnline)
        RecordDiscrepancyWithShill(network, status);
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      NOTREACHED();
      break;
  }
}

}  // namespace chromeos
