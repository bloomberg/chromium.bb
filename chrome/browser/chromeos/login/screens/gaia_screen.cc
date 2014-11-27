// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gaia_screen.h"

#include "base/logging.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

namespace chromeos {

GaiaScreen::GaiaScreen() : legacy_handler_(NULL), screen_handler_(NULL) {
}

GaiaScreen::~GaiaScreen() {
}

void GaiaScreen::SetLegacyHandler(LoginDisplayWebUIHandler* handler) {
  legacy_handler_ = handler;
}

void GaiaScreen::SetScreenHandler(GaiaScreenHandler* handler) {
  screen_handler_ = handler;
}

void GaiaScreen::MaybePreloadAuthExtension() {
  screen_handler_->MaybePreloadAuthExtension();
}

}  // namespace chromeos
