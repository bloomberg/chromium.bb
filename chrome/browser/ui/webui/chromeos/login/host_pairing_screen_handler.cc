// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/host_pairing_screen_handler.h"

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

namespace {

const char kJsScreenPath[] = "login.HostPairingScreen";

const char kMethodContextChanged[] = "contextChanged";

// Sent from JS when screen is ready to receive context updates.
// TODO(dzhioev): Move 'contextReady' logic to the base screen handler when
// all screens migrate to context-based communications.
const char kCallbackContextReady[] = "contextReady";

}  // namespace

HostPairingScreenHandler::HostPairingScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false),
      js_context_ready_(false) {
}

HostPairingScreenHandler::~HostPairingScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void HostPairingScreenHandler::HandleContextReady() {
  js_context_ready_ = true;
  OnContextChanged(context_cache_.storage());
}

void HostPairingScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void HostPairingScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
}

void HostPairingScreenHandler::RegisterMessages() {
  AddPrefixedCallback(kCallbackContextReady,
                      &HostPairingScreenHandler::HandleContextReady);
}

void HostPairingScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeUI::kScreenHostPairing, NULL);
}

void HostPairingScreenHandler::Hide() {
}

void HostPairingScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void HostPairingScreenHandler::OnContextChanged(
    const base::DictionaryValue& diff) {
  if (!js_context_ready_) {
    context_cache_.ApplyChanges(diff, NULL);
    return;
  }
  CallJS(kMethodContextChanged, diff);
}

}  // namespace chromeos
