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
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_status_code.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using captive_portal::CaptivePortalDetector;

namespace chromeos {

namespace {

// Delay before portal detection caused by changes in proxy settings.
const int kProxyChangeDelaySec = 1;

// Maximum number of reports from captive portal detector about
// offline state in a row before notification is sent to observers.
const int kMaxOfflineResultsBeforeReport = 3;

// Delay before portal detection attempt after !ONLINE -> !ONLINE
// transition.
const int kShortInitialDelayBetweenAttemptsMs = 600;

// Maximum timeout before portal detection attempts after !ONLINE ->
// !ONLINE transition.
const int kShortMaximumDelayBetweenAttemptsMs = 2 * 60 * 1000;

// Delay before portal detection attempt after !ONLINE -> ONLINE
// transition.
const int kLongInitialDelayBetweenAttemptsMs = 30 * 1000;

// Maximum timeout before portal detection attempts after !ONLINE ->
// ONLINE transition.
const int kLongMaximumDelayBetweenAttemptsMs = 5 * 60 * 1000;

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

bool InSession() {
  return LoginState::IsInitialized() && LoginState::Get()->IsUserLoggedIn();
}

void RecordDetectionResult(NetworkPortalDetector::CaptivePortalStatus status) {
  if (InSession()) {
    UMA_HISTOGRAM_ENUMERATION(
        NetworkPortalDetectorImpl::kSessionDetectionResultHistogram,
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        NetworkPortalDetectorImpl::kOobeDetectionResultHistogram,
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  }
}

void RecordDetectionDuration(const base::TimeDelta& duration) {
  if (InSession()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        NetworkPortalDetectorImpl::kSessionDetectionDurationHistogram,
        duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        NetworkPortalDetectorImpl::kOobeDetectionDurationHistogram, duration);
  }
}

void RecordDiscrepancyWithShill(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  if (InSession()) {
    if (network->connection_state() == shill::kStateOnline) {
      UMA_HISTOGRAM_ENUMERATION(
          NetworkPortalDetectorImpl::kSessionShillOnlineHistogram,
          status,
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
    } else if (network->connection_state() == shill::kStatePortal) {
      UMA_HISTOGRAM_ENUMERATION(
          NetworkPortalDetectorImpl::kSessionShillPortalHistogram,
          status,
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
    } else if (network->connection_state() == shill::kStateOffline) {
      UMA_HISTOGRAM_ENUMERATION(
          NetworkPortalDetectorImpl::kSessionShillOfflineHistogram,
          status,
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
    }
  } else {
    if (network->connection_state() == shill::kStateOnline) {
      UMA_HISTOGRAM_ENUMERATION(
          NetworkPortalDetectorImpl::kOobeShillOnlineHistogram,
          status,
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
    } else if (network->connection_state() == shill::kStatePortal) {
      UMA_HISTOGRAM_ENUMERATION(
          NetworkPortalDetectorImpl::kOobeShillPortalHistogram,
          status,
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
    } else if (network->connection_state() == shill::kStateOffline) {
      UMA_HISTOGRAM_ENUMERATION(
          NetworkPortalDetectorImpl::kOobeShillOfflineHistogram,
          status,
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
    }
  }
}

void RecordPortalToOnlineTransition(const base::TimeDelta& duration) {
  if (InSession()) {
    UMA_HISTOGRAM_LONG_TIMES(
        NetworkPortalDetectorImpl::kSessionPortalToOnlineHistogram,
        duration);
  } else {
    UMA_HISTOGRAM_LONG_TIMES(
        NetworkPortalDetectorImpl::kOobePortalToOnlineHistogram,
        duration);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl::DetectionAttemptCompletedLogState

NetworkPortalDetectorImpl::DetectionAttemptCompletedReport::
    DetectionAttemptCompletedReport()
    : result(captive_portal::RESULT_COUNT), response_code(-1) {
}

NetworkPortalDetectorImpl::DetectionAttemptCompletedReport::
    DetectionAttemptCompletedReport(const std::string network_name,
                                    const std::string network_id,
                                    captive_portal::CaptivePortalResult result,
                                    int response_code)
    : network_name(network_name),
      network_id(network_id),
      result(result),
      response_code(response_code) {
}

void NetworkPortalDetectorImpl::DetectionAttemptCompletedReport::Report()
    const {
  VLOG(1) << "Detection attempt completed: "
          << "name=" << network_name << ", "
          << "id=" << network_id << ", "
          << "result=" << captive_portal::CaptivePortalResultToString(result)
          << ", "
          << "response_code=" << response_code;
}

bool NetworkPortalDetectorImpl::DetectionAttemptCompletedReport::Equals(
    const DetectionAttemptCompletedReport& o) const {
  return network_name == o.network_name && network_id == o.network_id &&
         result == o.result && response_code == o.response_code;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, public:

const char NetworkPortalDetectorImpl::kOobeDetectionResultHistogram[] =
    "CaptivePortal.OOBE.DetectionResult";
const char NetworkPortalDetectorImpl::kOobeDetectionDurationHistogram[] =
    "CaptivePortal.OOBE.DetectionDuration";
const char NetworkPortalDetectorImpl::kOobeShillOnlineHistogram[] =
    "CaptivePortal.OOBE.DiscrepancyWithShill_Online";
const char NetworkPortalDetectorImpl::kOobeShillPortalHistogram[] =
    "CaptivePortal.OOBE.DiscrepancyWithShill_RestrictedPool";
const char NetworkPortalDetectorImpl::kOobeShillOfflineHistogram[] =
    "CaptivePortal.OOBE.DiscrepancyWithShill_Offline";
const char NetworkPortalDetectorImpl::kOobePortalToOnlineHistogram[] =
    "CaptivePortal.OOBE.PortalToOnlineTransition";

const char NetworkPortalDetectorImpl::kSessionDetectionResultHistogram[] =
    "CaptivePortal.Session.DetectionResult";
const char NetworkPortalDetectorImpl::kSessionDetectionDurationHistogram[] =
    "CaptivePortal.Session.DetectionDuration";
const char NetworkPortalDetectorImpl::kSessionShillOnlineHistogram[] =
    "CaptivePortal.Session.DiscrepancyWithShill_Online";
const char NetworkPortalDetectorImpl::kSessionShillPortalHistogram[] =
    "CaptivePortal.Session.DiscrepancyWithShill_RestrictedPool";
const char NetworkPortalDetectorImpl::kSessionShillOfflineHistogram[] =
    "CaptivePortal.Session.DiscrepancyWithShill_Offline";
const char NetworkPortalDetectorImpl::kSessionPortalToOnlineHistogram[] =
    "CaptivePortal.Session.PortalToOnlineTransition";

// static
void NetworkPortalDetectorImpl::Initialize(
    net::URLRequestContextGetter* url_context) {
  if (NetworkPortalDetector::set_for_testing())
    return;
  CHECK(!NetworkPortalDetector::network_portal_detector())
      << "NetworkPortalDetector was initialized twice.";
  if (CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType))
    set_network_portal_detector(new NetworkPortalDetectorStubImpl());
  else
    set_network_portal_detector(new NetworkPortalDetectorImpl(url_context));
}

NetworkPortalDetectorImpl::NetworkPortalDetectorImpl(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : state_(STATE_IDLE),
      test_url_(CaptivePortalDetector::kDefaultURL),
      enabled_(false),
      strategy_(PortalDetectorStrategy::CreateById(
          PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN, this)),
      last_detection_result_(CAPTIVE_PORTAL_STATUS_UNKNOWN),
      same_detection_result_count_(0),
      no_response_result_count_(0),
      weak_factory_(this) {
  captive_portal_detector_.reset(new CaptivePortalDetector(request_context));

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
  StartDetectionIfIdle();
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
    portal_state = GetCaptivePortalState(network->guid());
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
  VLOG(1) << "Starting detection for: "
          << "name=" << network->name() << ", id=" << network->guid();
  portal_state_map_.erase(network->guid());
  StartDetection();
}

NetworkPortalDetectorImpl::CaptivePortalState
NetworkPortalDetectorImpl::GetCaptivePortalState(const std::string& guid) {
  DCHECK(CalledOnValidThread());
  CaptivePortalStateMap::const_iterator it = portal_state_map_.find(guid);
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

void NetworkPortalDetectorImpl::SetStrategy(
    PortalDetectorStrategy::StrategyId id) {
  if (id == strategy_->Id())
    return;
  strategy_ = PortalDetectorStrategy::CreateById(id, this).Pass();
  StopDetection();
  StartDetectionIfIdle();
}

void NetworkPortalDetectorImpl::DefaultNetworkChanged(
    const NetworkState* default_network) {
  DCHECK(CalledOnValidThread());

  if (!default_network) {
    VLOG(1) << "DefaultNetworkChanged: None.";
    default_network_name_.clear();

    StopDetection();

    CaptivePortalState state;
    state.status = CAPTIVE_PORTAL_STATUS_OFFLINE;
    OnDetectionCompleted(NULL, state);
    return;
  }

  default_network_name_ = default_network->name();

  bool network_changed = (default_network_id_ != default_network->guid());
  default_network_id_ = default_network->guid();

  bool connection_state_changed =
      (default_connection_state_ != default_network->connection_state());
  default_connection_state_ = default_network->connection_state();

  VLOG(1) << "DefaultNetworkChanged: "
          << "name=" << default_network_name_ << ", "
          << "id=" << default_network_id_ << ", "
          << "state=" << default_connection_state_ << ", "
          << "changed=" << network_changed << ", "
          << "state_changed=" << connection_state_changed;

  if (network_changed || connection_state_changed)
    StopDetection();

  if (is_idle() && NetworkState::StateIsConnected(default_connection_state_)) {
    // Initiate Captive Portal detection if network's captive
    // portal state is unknown (e.g. for freshly created networks),
    // offline or if network connection state was changed.
    CaptivePortalState state = GetCaptivePortalState(default_network->guid());
    if (state.status == CAPTIVE_PORTAL_STATUS_UNKNOWN ||
        state.status == CAPTIVE_PORTAL_STATUS_OFFLINE ||
        (!network_changed && connection_state_changed)) {
      ScheduleAttempt(base::TimeDelta());
    }
  }
}

int NetworkPortalDetectorImpl::NoResponseResultCount() {
  return no_response_result_count_;
}

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
  DCHECK(is_idle());

  ResetStrategyAndCounters();
  detection_start_time_ = GetCurrentTimeTicks();
  ScheduleAttempt(base::TimeDelta());
}

void NetworkPortalDetectorImpl::StopDetection() {
  attempt_task_.Cancel();
  attempt_timeout_.Cancel();
  captive_portal_detector_->Cancel();
  state_ = STATE_IDLE;
  ResetStrategyAndCounters();
}

void NetworkPortalDetectorImpl::ScheduleAttempt(const base::TimeDelta& delay) {
  DCHECK(is_idle());

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
  DCHECK(CalledOnValidThread());
  DCHECK(is_checking_for_portal());

  captive_portal::CaptivePortalResult result = results.result;
  int response_code = results.response_code;

  const NetworkState* network = DefaultNetwork();

  // If using a fake profile client, also fake being behind a captive portal
  // if the default network is in portal state.
  if (result != captive_portal::RESULT_NO_RESPONSE &&
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface() &&
      network && network->connection_state() == shill::kStatePortal) {
    result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;
    response_code = 200;
  }

  DetectionAttemptCompletedReport attempt_completed_report(
      default_network_name_, default_network_id_, result, response_code);
  if (!attempt_completed_report_.Equals(attempt_completed_report)) {
    attempt_completed_report_ = attempt_completed_report;
    attempt_completed_report_.Report();
  }

  state_ = STATE_IDLE;
  attempt_timeout_.Cancel();

  CaptivePortalState state;
  state.response_code = response_code;
  state.time = GetCurrentTimeTicks();
  switch (result) {
    case captive_portal::RESULT_NO_RESPONSE:
      if (state.response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
        state.status = CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
      } else if (network &&
                 (network->connection_state() == shill::kStatePortal)) {
        // Take into account shill's detection results.
        state.status = CAPTIVE_PORTAL_STATUS_PORTAL;
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

  if (last_detection_result_ != state.status) {
    last_detection_result_ = state.status;
    same_detection_result_count_ = 1;
    net::BackoffEntry::Policy policy = strategy_->policy();
    if (state.status == CAPTIVE_PORTAL_STATUS_ONLINE) {
      policy.initial_delay_ms = kLongInitialDelayBetweenAttemptsMs;
      policy.maximum_backoff_ms = kLongMaximumDelayBetweenAttemptsMs;
    } else {
      policy.initial_delay_ms = kShortInitialDelayBetweenAttemptsMs;
      policy.maximum_backoff_ms = kShortMaximumDelayBetweenAttemptsMs;
    }
    strategy_->SetPolicyAndReset(policy);
  } else {
    ++same_detection_result_count_;
  }
  strategy_->OnDetectionCompleted();

  if (result == captive_portal::RESULT_NO_RESPONSE)
    ++no_response_result_count_;
  else
    no_response_result_count_ = 0;

  if (state.status != CAPTIVE_PORTAL_STATUS_OFFLINE ||
      same_detection_result_count_ >= kMaxOfflineResultsBeforeReport) {
    OnDetectionCompleted(network, state);
  }
  ScheduleAttempt(results.retry_after_delta);
}

void NetworkPortalDetectorImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_LOGIN_PROXY_CHANGED ||
      type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
      type == chrome::NOTIFICATION_AUTH_CANCELLED) {
    VLOG(1) << "Restarting portal detection due to proxy change.";
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
      portal_state_map_.find(network->guid());
  if (it == portal_state_map_.end() || it->second.status != state.status ||
      it->second.response_code != state.response_code) {
    // Record detection duration iff detection result differs from the
    // previous one for this network. The reason is to record all stats
    // only when network changes it's state.
    RecordDetectionStats(network, state.status);
    if (it != portal_state_map_.end() &&
        it->second.status == CAPTIVE_PORTAL_STATUS_PORTAL &&
        state.status == CAPTIVE_PORTAL_STATUS_ONLINE) {
      RecordPortalToOnlineTransition(state.time - it->second.time);
    }

    portal_state_map_[network->guid()] = state;
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

  if (!detection_start_time_.is_null())
    RecordDetectionDuration(GetCurrentTimeTicks() - detection_start_time_);
  RecordDetectionResult(status);

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

void NetworkPortalDetectorImpl::ResetStrategyAndCounters() {
  last_detection_result_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  same_detection_result_count_ = 0;
  no_response_result_count_ = 0;
  strategy_->Reset();
}

}  // namespace chromeos
