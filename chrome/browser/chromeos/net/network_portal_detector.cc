// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "net/http/http_status_code.h"
#include "ui/base/l10n/l10n_util.h"

using captive_portal::CaptivePortalDetector;

namespace chromeos {

namespace {

// Maximum number of portal detections for the same active network
// after network change.
const int kMaxRequestAttempts = 3;

// Minimum timeout between consecutive portal checks for the same
// network.
const int kMinTimeBetweenAttemptsSec = 3;

// Timeout for a portal check.
const int kRequestTimeoutSec = 10;

// Delay before portal detection caused by changes in proxy settings.
const int kProxyChangeDelayMs = 1000;

// Delay between consecutive portal checks for a network in lazy mode.
// TODO (ygorshenin@): use exponential backoff or normally distributed
// random variable instead of this.
const int kLazyCheckIntervalSec = 30;

std::string CaptivePortalStatusString(
    NetworkPortalDetector::CaptivePortalStatus status) {
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_CAPTIVE_PORTAL_STATUS_UNKNOWN);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_CAPTIVE_PORTAL_STATUS_OFFLINE);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_CAPTIVE_PORTAL_STATUS_ONLINE);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_CAPTIVE_PORTAL_STATUS_PORTAL);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      NOTREACHED();
  }
  return l10n_util::GetStringUTF8(
      IDS_CHROMEOS_CAPTIVE_PORTAL_STATUS_UNRECOGNIZED);
}

NetworkPortalDetector* g_network_portal_detector = NULL;

}  // namespace

NetworkPortalDetector::NetworkPortalDetector(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : active_connection_state_(STATE_UNKNOWN),
      test_url_(CaptivePortalDetector::kDefaultURL),
      weak_ptr_factory_(this),
      attempt_count_(0),
      lazy_detection_enabled_(false),
      lazy_check_interval_(base::TimeDelta::FromSeconds(kLazyCheckIntervalSec)),
      min_time_between_attempts_(
          base::TimeDelta::FromSeconds(kMinTimeBetweenAttemptsSec)),
      request_timeout_(base::TimeDelta::FromSeconds(kRequestTimeoutSec)) {
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
}

NetworkPortalDetector::~NetworkPortalDetector() {
}

void NetworkPortalDetector::Init() {
  DCHECK(CalledOnValidThread());
  DCHECK(chromeos::CrosLibrary::Get());

  state_ = STATE_IDLE;
  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  DCHECK(network_library);
  network_library->AddNetworkManagerObserver(this);
  network_library->RemoveObserverForAllNetworks(this);
}

void NetworkPortalDetector::Shutdown() {
  DCHECK(CalledOnValidThread());
  DCHECK(chromeos::CrosLibrary::Get());

  detection_task_.Cancel();
  detection_timeout_.Cancel();

  captive_portal_detector_->Cancel();
  captive_portal_detector_.reset();
  observers_.Clear();
  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_library)
    network_library->RemoveNetworkManagerObserver(this);
}

void NetworkPortalDetector::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());

  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkPortalDetector::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());

  observers_.RemoveObserver(observer);
}

NetworkPortalDetector::CaptivePortalState
NetworkPortalDetector::GetCaptivePortalState(const Network* network) {
  DCHECK(CalledOnValidThread());

  if (!network)
    return CaptivePortalState();
  CaptivePortalStateMap::const_iterator it =
      portal_state_map_.find(network->service_path());
  if (it == portal_state_map_.end())
    return CaptivePortalState();
  return it->second;
}

void NetworkPortalDetector::OnNetworkManagerChanged(NetworkLibrary* cros) {
  DCHECK(CalledOnValidThread());

  const Network* active_network = cros->active_network();
  if (!active_network)
    return;

  active_network_id_ = active_network->unique_id();

  bool network_changed =
      (active_service_path_ != active_network->service_path());
  if (network_changed) {
    if (!active_service_path_.empty())
      cros->RemoveNetworkObserver(active_service_path_, this);
    active_service_path_ = active_network->service_path();
    cros->AddNetworkObserver(active_service_path_, this);
  }

  bool connection_state_changed =
      (active_connection_state_ != active_network->connection_state());
  active_connection_state_ = active_network->connection_state();

  if (network_changed || connection_state_changed) {
    attempt_count_ = 0;
    CancelPortalDetection();
  }

  if (!IsCheckingForPortal() && !IsPortalCheckPending() &&
      Network::IsConnectedState(active_connection_state_) &&
      (attempt_count_ < kMaxRequestAttempts || lazy_detection_enabled_)) {
    DCHECK(active_network);

    // Initiate Captive Portal detection if network's captive
    // portal state is unknown (e.g. for freshly created networks),
    // offline or if network connection state was changed.
    CaptivePortalState state = GetCaptivePortalState(active_network);
    if (state.status == CAPTIVE_PORTAL_STATUS_UNKNOWN ||
        state.status == CAPTIVE_PORTAL_STATUS_OFFLINE ||
        (!network_changed && connection_state_changed)) {
      DetectCaptivePortal(base::TimeDelta());
    }
  }
}

void NetworkPortalDetector::OnNetworkChanged(chromeos::NetworkLibrary* cros,
                                             const chromeos::Network* network) {
  DCHECK(CalledOnValidThread());
  OnNetworkManagerChanged(cros);
}

void NetworkPortalDetector::EnableLazyDetection() {
  if (lazy_detection_enabled_)
    return;
  VLOG(1) << "Lazy detection mode enabled";
  lazy_detection_enabled_ = true;
  if (!IsPortalCheckPending() && !IsCheckingForPortal())
    DetectCaptivePortal(base::TimeDelta());
}

void NetworkPortalDetector::DisableLazyDetection() {
  if (!lazy_detection_enabled_)
    return;
  VLOG(1) << "Lazy detection mode disabled";
  if (attempt_count_ == kMaxRequestAttempts)
    CancelPortalDetection();
  lazy_detection_enabled_ = false;
}

// static
NetworkPortalDetector* NetworkPortalDetector::CreateInstance() {
  DCHECK(!g_network_portal_detector);
  g_network_portal_detector = new NetworkPortalDetector(
      g_browser_process->system_request_context());
  return g_network_portal_detector;
}

// static
NetworkPortalDetector* NetworkPortalDetector::GetInstance() {
  if (!g_network_portal_detector)
    return CreateInstance();
  return g_network_portal_detector;
}

// static
bool NetworkPortalDetector::IsEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableChromeCaptivePortalDetector);
}

void NetworkPortalDetector::DetectCaptivePortal(const base::TimeDelta& delay) {
  DCHECK(!IsPortalCheckPending());
  DCHECK(!IsCheckingForPortal());
  DCHECK(attempt_count_ < kMaxRequestAttempts || lazy_detection_enabled_);

  detection_task_.Cancel();
  detection_timeout_.Cancel();
  state_ = STATE_PORTAL_CHECK_PENDING;

  next_attempt_delay_ = delay;
  if (attempt_count_ > 0) {
    base::TimeTicks now = GetCurrentTimeTicks();
    base::TimeDelta elapsed_time;

    base::TimeDelta delay_between_attempts = min_time_between_attempts_;
    if (attempt_count_ == kMaxRequestAttempts) {
      DCHECK(lazy_detection_enabled_);
      delay_between_attempts = lazy_check_interval_;
    }
    if (now > attempt_start_time_)
      elapsed_time = now - attempt_start_time_;
    if (elapsed_time < delay_between_attempts &&
        delay_between_attempts - elapsed_time > next_attempt_delay_) {
      next_attempt_delay_ = delay_between_attempts - elapsed_time;
    }
  } else {
    detection_start_time_ = GetCurrentTimeTicks();
  }
  detection_task_.Reset(
      base::Bind(&NetworkPortalDetector::DetectCaptivePortalTask,
                 weak_ptr_factory_.GetWeakPtr()));
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          detection_task_.callback(),
                                          next_attempt_delay_);
}

void NetworkPortalDetector::DetectCaptivePortalTask() {
  DCHECK(IsPortalCheckPending());

  state_ = STATE_CHECKING_FOR_PORTAL;

  attempt_start_time_ = GetCurrentTimeTicks();

  if (attempt_count_ < kMaxRequestAttempts) {
    ++attempt_count_;
    VLOG(1) << "Portal detection started: "
            << "network=" << active_network_id_ << ", "
            << "attempt=" << attempt_count_ << " of " << kMaxRequestAttempts;
  } else {
    DCHECK(lazy_detection_enabled_);
    VLOG(1) << "Lazy portal detection attempt started";
  }

  captive_portal_detector_->DetectCaptivePortal(
      test_url_,
      base::Bind(&NetworkPortalDetector::OnPortalDetectionCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
  detection_timeout_.Reset(
      base::Bind(&NetworkPortalDetector::PortalDetectionTimeout,
                 weak_ptr_factory_.GetWeakPtr()));
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          detection_timeout_.callback(),
                                          request_timeout_);
}

void NetworkPortalDetector::PortalDetectionTimeout() {
  DCHECK(CalledOnValidThread());
  DCHECK(IsCheckingForPortal());

  VLOG(1) << "Portal detection timeout: network=" << active_network_id_;

  captive_portal_detector_->Cancel();
  CaptivePortalDetector::Results results;
  results.result = captive_portal::RESULT_NO_RESPONSE;
  OnPortalDetectionCompleted(results);
}

void NetworkPortalDetector::CancelPortalDetection() {
  if (IsPortalCheckPending()) {
    detection_task_.Cancel();
    detection_timeout_.Cancel();
  } else if (IsCheckingForPortal()) {
    captive_portal_detector_->Cancel();
  }
  state_ = STATE_IDLE;
}

void NetworkPortalDetector::OnPortalDetectionCompleted(
    const CaptivePortalDetector::Results& results) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsCheckingForPortal());

  VLOG(1) << "Portal detection completed: "
          << "network=" << active_network_id_ << ", "
          << "result=" << CaptivePortalDetector::CaptivePortalResultToString(
              results.result) << ", "
          << "response_code=" << results.response_code;

  state_ = STATE_IDLE;
  detection_timeout_.Cancel();

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const Network* active_network = cros->active_network();
  if (!active_network) {
    TryLazyDetection();
    return;
  }

  CaptivePortalState state;
  state.response_code = results.response_code;
  switch (results.result) {
    case captive_portal::RESULT_NO_RESPONSE:
      if (attempt_count_ >= kMaxRequestAttempts) {
        if (state.response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
          state.status = CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
        } else if (active_network->restricted_pool()) {
          // Take into account shill's detection results.
          state.status = CAPTIVE_PORTAL_STATUS_PORTAL;
          LOG(WARNING) << "Network " << active_network->unique_id() << " "
                       << "is marked as "
                       << CaptivePortalStatusString(state.status) << " "
                       << "despite the fact that CaptivePortalDetector "
                       << "received no response";
        } else {
          state.status = CAPTIVE_PORTAL_STATUS_OFFLINE;
        }
        SetCaptivePortalState(active_network, state);
      } else {
        DetectCaptivePortal(results.retry_after_delta);
      }
      break;
    case captive_portal::RESULT_INTERNET_CONNECTED:
      state.status = CAPTIVE_PORTAL_STATUS_ONLINE;
      SetCaptivePortalState(active_network, state);
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      state.status = CAPTIVE_PORTAL_STATUS_PORTAL;
      SetCaptivePortalState(active_network, state);
      break;
    default:
      break;
  }

  TryLazyDetection();
}

void NetworkPortalDetector::TryLazyDetection() {
  if (!IsPortalCheckPending() && !IsCheckingForPortal() &&
      lazy_detection_enabled_) {
    attempt_count_ = kMaxRequestAttempts;
    DetectCaptivePortal(base::TimeDelta());
  }
}

void NetworkPortalDetector::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_LOGIN_PROXY_CHANGED ||
      type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
      type == chrome::NOTIFICATION_AUTH_CANCELLED) {
    attempt_count_ = 0;
    if (!IsCheckingForPortal() && !IsPortalCheckPending()) {
      DetectCaptivePortal(
          base::TimeDelta::FromMilliseconds(kProxyChangeDelayMs));
    }
  }
}

bool NetworkPortalDetector::IsPortalCheckPending() const {
  return state_ == STATE_PORTAL_CHECK_PENDING;
}

bool NetworkPortalDetector::IsCheckingForPortal() const {
  return state_ == STATE_CHECKING_FOR_PORTAL;
}

void NetworkPortalDetector::SetCaptivePortalState(
    const Network* network,
    const CaptivePortalState& state) {
  DCHECK(network);

  if (!detection_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("CaptivePortal.OOBE.DetectionDuration",
                        GetCurrentTimeTicks() - detection_start_time_);
  }

  CaptivePortalStateMap::const_iterator it =
      portal_state_map_.find(network->service_path());
  if (it == portal_state_map_.end() ||
      it->second.status != state.status ||
      it->second.response_code != state.response_code) {
    VLOG(1) << "Updating Chrome Captive Portal state: "
            << "network=" << network->unique_id() << ", "
            << "status=" << CaptivePortalStatusString(state.status) << ", "
            << "response_code=" << state.response_code;
    portal_state_map_[network->service_path()] = state;
    NotifyPortalStateChanged(network, state);
  }
}

void NetworkPortalDetector::NotifyPortalStateChanged(
    const Network* network,
    const CaptivePortalState& state) {
  FOR_EACH_OBSERVER(Observer, observers_, OnPortalStateChanged(network, state));
}

base::TimeTicks NetworkPortalDetector::GetCurrentTimeTicks() const {
  if (time_ticks_for_testing_.is_null())
    return base::TimeTicks::Now();
  else
    return time_ticks_for_testing_;
}

}  // namespace chromeos
