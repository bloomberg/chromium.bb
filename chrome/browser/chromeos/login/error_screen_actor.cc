// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/error_screen_actor.h"

namespace chromeos {

// static
const char ErrorScreenActor::kErrorReasonProxyAuthCancelled[] =
    "frame error:111";
const char ErrorScreenActor::kErrorReasonProxyAuthSupplied[] =
    "proxy auth supplied";
const char ErrorScreenActor::kErrorReasonProxyConnectionFailed[] =
    "frame error:130";
const char ErrorScreenActor::kErrorReasonProxyConfigChanged[] =
    "proxy changed";
const char ErrorScreenActor::kErrorReasonLoadingTimeout[] = "loading timeout";
const char ErrorScreenActor::kErrorReasonPortalDetected[] = "portal detected";
const char ErrorScreenActor::kErrorReasonNetworkChanged[] = "network changed";
const char ErrorScreenActor::kErrorReasonUpdate[] = "update";

ErrorScreenActor::ErrorScreenActor()
    : ui_state_(ErrorScreen::UI_STATE_UNKNOWN),
      error_state_(ErrorScreen::ERROR_STATE_UNKNOWN),
      parent_screen_(OobeUI::SCREEN_UNKNOWN) {
}

ErrorScreenActor::~ErrorScreenActor() {
}

}  // namespace chromeos
