// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/network_screen.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Time in seconds for connection timeout.
const int kConnectionTimeoutSec = 40;

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

NetworkScreen::NetworkScreen(ScreenObserver* screen_observer,
                             NetworkScreenActor* actor)
    : WizardScreen(screen_observer),
      is_network_subscribed_(false),
      continue_pressed_(false),
      actor_(actor),
      network_state_helper_(new login::NetworkStateHelper) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

NetworkScreen::~NetworkScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
  connection_timer_.Stop();
  UnsubscribeNetworkNotification();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, WizardScreen implementation:

void NetworkScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void NetworkScreen::Show() {
  Refresh();
  if (actor_)
    actor_->Show();
}

void NetworkScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string NetworkScreen::GetName() const {
  return WizardController::kNetworkScreenName;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, NetworkStateHandlerObserver implementation:

void NetworkScreen::NetworkConnectionStateChanged(const NetworkState* network) {
  UpdateStatus();
}

void NetworkScreen::DefaultNetworkChanged(const NetworkState* network) {
  UpdateStatus();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

void NetworkScreen::Refresh() {
  SubscribeNetworkNotification();
  UpdateStatus();
}

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, NetworkScreenActor::Delegate implementation:

void NetworkScreen::OnActorDestroyed(NetworkScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void NetworkScreen::OnContinuePressed() {
  if (network_state_helper_->IsConnected()) {
    NotifyOnConnection();
  } else {
    continue_pressed_ = true;
    WaitForConnection(network_id_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, private:

void NetworkScreen::SetNetworkStateHelperForTest(
    login::NetworkStateHelper* helper) {
  network_state_helper_.reset(helper);
}

void NetworkScreen::SubscribeNetworkNotification() {
  if (!is_network_subscribed_) {
    is_network_subscribed_ = true;
    NetworkHandler::Get()->network_state_handler()->AddObserver(
        this, FROM_HERE);
  }
}

void NetworkScreen::UnsubscribeNetworkNotification() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

void NetworkScreen::NotifyOnConnection() {
  // TODO(nkostylev): Check network connectivity.
  UnsubscribeNetworkNotification();
  connection_timer_.Stop();
  get_screen_observer()->OnExit(ScreenObserver::NETWORK_CONNECTED);
}

void NetworkScreen::OnConnectionTimeout() {
  StopWaitingForConnection(network_id_);
  if (!network_state_helper_->IsConnected() && actor_) {
    // Show error bubble.
    actor_->ShowError(
        l10n_util::GetStringFUTF16(
            IDS_NETWORK_SELECTION_ERROR,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME),
            network_id_));
  }
}

void NetworkScreen::UpdateStatus() {
  if (!actor_)
    return;

  bool is_connected = network_state_helper_->IsConnected();
  if (is_connected)
    actor_->ClearErrors();

  base::string16 network_name = network_state_helper_->GetCurrentNetworkName();
  if (is_connected) {
    StopWaitingForConnection(network_name);
  } else if (network_state_helper_->IsConnecting()) {
    WaitForConnection(network_name);
  } else {
    StopWaitingForConnection(network_id_);
  }
}

void NetworkScreen::StopWaitingForConnection(const base::string16& network_id) {
  bool is_connected = network_state_helper_->IsConnected();
  if (is_connected && continue_pressed_) {
    NotifyOnConnection();
    return;
  }

  continue_pressed_ = false;
  connection_timer_.Stop();

  network_id_ = network_id;
  if (actor_) {
    actor_->ShowConnectingStatus(false, network_id_);
    actor_->EnableContinue(is_connected);
  }
}

void NetworkScreen::WaitForConnection(const base::string16& network_id) {
  if (network_id_ != network_id || !connection_timer_.IsRunning()) {
    connection_timer_.Stop();
    connection_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromSeconds(kConnectionTimeoutSec),
                            this,
                            &NetworkScreen::OnConnectionTimeout);
  }

  network_id_ = network_id;
  if (actor_) {
    actor_->ShowConnectingStatus(continue_pressed_, network_id_);
    actor_->EnableContinue(false);
  }
}

}  // namespace chromeos
