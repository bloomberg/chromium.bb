// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
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

std::string CaptivePortalStateString(
    NetworkPortalDetector::CaptivePortalState state) {
  switch (state) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATE_UNKNOWN:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_CAPTIVE_PORTAL_STATE_UNKNOWN);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATE_OFFLINE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_CAPTIVE_PORTAL_STATE_OFFLINE);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATE_ONLINE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_CAPTIVE_PORTAL_STATE_ONLINE);
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATE_PORTAL:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_CAPTIVE_PORTAL_STATE_PORTAL);
  }
  return l10n_util::GetStringUTF8(
      IDS_CHROMEOS_CAPTIVE_PORTAL_STATE_UNRECOGNIZED);
}

NetworkPortalDetector* g_network_portal_detector = NULL;

}  // namespace

NetworkPortalDetector::NetworkPortalDetector(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : active_connection_state_(STATE_UNKNOWN),
      test_url_(CaptivePortalDetector::kDefaultURL),
      weak_ptr_factory_(this),
      attempt_count_(0),
      min_time_between_attempts_(
          base::TimeDelta::FromSeconds(kMinTimeBetweenAttemptsSec)),
      request_timeout_(base::TimeDelta::FromSeconds(kRequestTimeoutSec)) {
  captive_portal_detector_.reset(new CaptivePortalDetector(request_context));
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
    return CAPTIVE_PORTAL_STATE_UNKNOWN;
  CaptivePortalStateMap::const_iterator it =
      captive_portal_state_map_.find(network->service_path());
  if (it == captive_portal_state_map_.end())
    return CAPTIVE_PORTAL_STATE_UNKNOWN;
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
    if (IsPortalCheckPending()) {
      detection_task_.Cancel();
      detection_timeout_.Cancel();
    } else if (IsCheckingForPortal()) {
      captive_portal_detector_->Cancel();
    }
    state_ = STATE_IDLE;
  }

  if (!IsCheckingForPortal() && !IsPortalCheckPending() &&
      Network::IsConnectedState(active_connection_state_) &&
      attempt_count_ < kMaxRequestAttempts) {
    DCHECK(active_network);

    // Initiate Captive Portal detection if network's captive
    // portal state is unknown (e.g. for freshly created networks),
    // offline or if network connection state was changed.
    CaptivePortalState state = GetCaptivePortalState(active_network);
    if (state == CAPTIVE_PORTAL_STATE_UNKNOWN ||
        state == CAPTIVE_PORTAL_STATE_OFFLINE ||
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
  DCHECK(attempt_count_ < kMaxRequestAttempts);

  detection_task_.Cancel();
  detection_timeout_.Cancel();
  state_ = STATE_PORTAL_CHECK_PENDING;

  next_attempt_delay_ = delay;
  if (attempt_count_ > 0) {
    base::TimeTicks now = GetCurrentTimeTicks();
    base::TimeDelta elapsed_time;
    if (now > attempt_start_time_)
      elapsed_time = now - attempt_start_time_;
    if (elapsed_time < min_time_between_attempts_ &&
        min_time_between_attempts_ - elapsed_time > next_attempt_delay_) {
      next_attempt_delay_ = min_time_between_attempts_ - elapsed_time;
    }
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

  ++attempt_count_;
  attempt_start_time_ = GetCurrentTimeTicks();

  VLOG(1) << "Portal detection started: network=" << active_network_id_ << ", "
          << "attempt=" << attempt_count_ << " of " << kMaxRequestAttempts;

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

void NetworkPortalDetector::OnPortalDetectionCompleted(
    const CaptivePortalDetector::Results& results) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsCheckingForPortal());

  VLOG(1) << "Portal detection completed: "
          << "network=" << active_network_id_ << ", "
          << "result=" << CaptivePortalDetector::CaptivePortalResultToString(
              results.result);

  state_ = STATE_IDLE;
  detection_timeout_.Cancel();

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const Network* active_network = cros->active_network();
  if (!active_network)
    return;

  switch (results.result) {
    case captive_portal::RESULT_NO_RESPONSE:
      if (attempt_count_ >= kMaxRequestAttempts)
        SetCaptivePortalState(active_network, CAPTIVE_PORTAL_STATE_OFFLINE);
      else
        DetectCaptivePortal(results.retry_after_delta);
      break;
    case captive_portal::RESULT_INTERNET_CONNECTED:
      SetCaptivePortalState(active_network, CAPTIVE_PORTAL_STATE_ONLINE);
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      SetCaptivePortalState(active_network, CAPTIVE_PORTAL_STATE_PORTAL);
      break;
    default:
      break;
  }
}

bool NetworkPortalDetector::IsPortalCheckPending() const {
  return state_ == STATE_PORTAL_CHECK_PENDING;
}

bool NetworkPortalDetector::IsCheckingForPortal() const {
  return state_ == STATE_CHECKING_FOR_PORTAL;
}

void NetworkPortalDetector::SetCaptivePortalState(const Network* network,
                                                  CaptivePortalState state) {
  DCHECK(network);

  CaptivePortalStateMap::const_iterator it =
      captive_portal_state_map_.find(network->service_path());
  if (it == captive_portal_state_map_.end() ||
      it->second != state) {
    VLOG(1) << "Updating Chrome Captive Portal state: "
            << "network=" << network->unique_id() << ", "
            << "state=" << CaptivePortalStateString(state);
    captive_portal_state_map_[network->service_path()] = state;
    NotifyPortalStateChanged(network, state);
  }
}

void NetworkPortalDetector::NotifyPortalStateChanged(const Network* network,
                                                     CaptivePortalState state) {
  FOR_EACH_OBSERVER(Observer, observers_, OnPortalStateChanged(network, state));
}

base::TimeTicks NetworkPortalDetector::GetCurrentTimeTicks() const {
  if (time_ticks_for_testing_.is_null())
    return base::TimeTicks::Now();
  else
    return time_ticks_for_testing_;
}

}  // namespace chromeos
