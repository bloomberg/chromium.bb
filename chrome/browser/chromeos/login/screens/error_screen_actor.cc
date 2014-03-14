// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"

namespace chromeos {

namespace {

const char kErrorReasonProxyAuthCancelled[] = "proxy auth cancelled";
const char kErrorReasonProxyAuthSupplied[] = "proxy auth supplied";
const char kErrorReasonProxyConnectionFailed[] = "proxy connection failed";
const char kErrorReasonProxyConfigChanged[] = "proxy config changed";
const char kErrorReasonLoadingTimeout[] = "loading timeout";
const char kErrorReasonPortalDetected[] = "portal detected";
const char kErrorReasonNetworkStateChanged[] = "network state changed";
const char kErrorReasonUpdate[] = "update";
const char kErrorReasonFrameError[] = "frame error";

}  // namespace

ErrorScreenActor::ErrorScreenActor()
    : ui_state_(ErrorScreen::UI_STATE_UNKNOWN),
      error_state_(ErrorScreen::ERROR_STATE_UNKNOWN),
      guest_signin_allowed_(false),
      offline_login_allowed_(false),
      show_connecting_indicator_(false),
      parent_screen_(OobeUI::SCREEN_UNKNOWN) {}

ErrorScreenActor::~ErrorScreenActor() {}

// static
const char* ErrorScreenActor::ErrorReasonString(ErrorReason reason) {
  switch (reason) {
    case ErrorScreenActor::ERROR_REASON_PROXY_AUTH_CANCELLED:
      return kErrorReasonProxyAuthCancelled;
    case ErrorScreenActor::ERROR_REASON_PROXY_AUTH_SUPPLIED:
      return kErrorReasonProxyAuthSupplied;
    case ErrorScreenActor::ERROR_REASON_PROXY_CONNECTION_FAILED:
      return kErrorReasonProxyConnectionFailed;
    case ErrorScreenActor::ERROR_REASON_PROXY_CONFIG_CHANGED:
      return kErrorReasonProxyConfigChanged;
    case ErrorScreenActor::ERROR_REASON_LOADING_TIMEOUT:
      return kErrorReasonLoadingTimeout;
    case ErrorScreenActor::ERROR_REASON_PORTAL_DETECTED:
      return kErrorReasonPortalDetected;
    case ErrorScreenActor::ERROR_REASON_NETWORK_STATE_CHANGED:
      return kErrorReasonNetworkStateChanged;
    case ErrorScreenActor::ERROR_REASON_UPDATE:
      return kErrorReasonUpdate;
    case ErrorScreenActor::ERROR_REASON_FRAME_ERROR:
      return kErrorReasonFrameError;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace chromeos
