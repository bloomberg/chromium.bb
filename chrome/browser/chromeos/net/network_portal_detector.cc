// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using captive_portal::CaptivePortalDetector;

namespace chromeos {

namespace {

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
    : test_url_(CaptivePortalDetector::kDefaultURL) {
  captive_portal_detector_.reset(
      new CaptivePortalDetector(request_context));
}

NetworkPortalDetector::~NetworkPortalDetector() {
}

void NetworkPortalDetector::Init() {
  DCHECK(CalledOnValidThread());

  state_ = STATE_IDLE;
  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  network_library->AddNetworkManagerObserver(this);
}

void NetworkPortalDetector::Shutdown() {
  DCHECK(CalledOnValidThread());

  captive_portal_detector_->Cancel();
  captive_portal_detector_.reset();
  observers_.Clear();
  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
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
  if (!network)
    return CAPTIVE_PORTAL_STATE_UNKNOWN;
  CaptivePortalStateMap::const_iterator it =
      captive_portal_state_map_.find(network->unique_id());
  if (it == captive_portal_state_map_.end())
    return CAPTIVE_PORTAL_STATE_UNKNOWN;
  return it->second;
}

void NetworkPortalDetector::OnNetworkManagerChanged(NetworkLibrary* cros) {
  DCHECK(CalledOnValidThread());

  // Suppose that if there are no active network, then unique id is
  // empty and connection state is unknown.
  std::string new_active_network_id;
  ConnectionState connection_state = STATE_UNKNOWN;
  const Network* active_network = cros->active_network();
  if (active_network) {
    new_active_network_id = active_network->unique_id();
    connection_state = active_network->connection_state();
  }

  if (active_network_id_ != new_active_network_id) {
    active_network_id_ = new_active_network_id;
    if (IsCheckingForPortal()) {
      // Network is changed, so detection results will be incorrect.
      state_ = STATE_CHECKING_FOR_PORTAL_NETWORK_CHANGED;
    } else if (Network::IsConnectedState(connection_state)) {
      // Start captive portal detection, if possible.
      DetectCaptivePortal();
    }
  } else if (!IsCheckingForPortal() &&
             Network::IsConnectedState(connection_state)) {
    DCHECK(active_network);
    // Initiate Captive Portal detection only if network's captive
    // portal state is unknown (e.g. for freshly created networks) or offline.
    CaptivePortalState state = GetCaptivePortalState(active_network);
    if (state == CAPTIVE_PORTAL_STATE_UNKNOWN ||
        state == CAPTIVE_PORTAL_STATE_OFFLINE) {
      DetectCaptivePortal();
    }
  }
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

void NetworkPortalDetector::DetectCaptivePortal() {
  state_ = STATE_CHECKING_FOR_PORTAL;
  captive_portal_detector_->DetectCaptivePortal(
      test_url_,
      base::Bind(&NetworkPortalDetector::OnPortalDetectionCompleted,
                 base::Unretained(this)));
}

void NetworkPortalDetector::OnPortalDetectionCompleted(
    const CaptivePortalDetector::Results& results) {
  DCHECK(CalledOnValidThread());

  DCHECK(IsCheckingForPortal());

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const Network* active_network = cros->active_network();
  if (!active_network)
    return;

  if (state_ == STATE_CHECKING_FOR_PORTAL_NETWORK_CHANGED) {
    // Can't use detection results, because network was changed. So
    // retry portal detection for the active network or, if possible.
    if (Network::IsConnectedState(active_network->connection_state()) &&
        GetCaptivePortalState(active_network) ==
        CAPTIVE_PORTAL_STATE_UNKNOWN) {
      DetectCaptivePortal();
    }
    return;
  }

  state_ = STATE_IDLE;

  switch (results.result) {
    case captive_portal::RESULT_NO_RESPONSE:
      SetCaptivePortalState(active_network, CAPTIVE_PORTAL_STATE_OFFLINE);
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

bool NetworkPortalDetector::IsCheckingForPortal() const {
  return state_ == STATE_CHECKING_FOR_PORTAL ||
      state_ == STATE_CHECKING_FOR_PORTAL_NETWORK_CHANGED;
}

void NetworkPortalDetector::SetCaptivePortalState(const Network* network,
                                                  CaptivePortalState state) {
  DCHECK(network);
  CaptivePortalStateMap::const_iterator it =
      captive_portal_state_map_.find(network->unique_id());
  if (it == captive_portal_state_map_.end() ||
      it->second != state) {
    VLOG(2) << "Updating Chrome Captive Portal state: "
            << "network=" << network->unique_id() << ", "
            << "state=" << CaptivePortalStateString(state);
    captive_portal_state_map_[network->unique_id()] = state;
    NotifyPortalStateChanged(network, state);
  }
}

void NetworkPortalDetector::NotifyPortalStateChanged(const Network* network,
                                                     CaptivePortalState state) {
  FOR_EACH_OBSERVER(Observer, observers_, OnPortalStateChanged(network, state));
}

}  // namespace chromeos
