// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oobe_screen.h"

#include <ostream>

namespace chromeos {

OobeScreenId::OobeScreenId(const std::string& name) : name(name) {}

OobeScreenId::OobeScreenId(const StaticOobeScreenId& id) : name(id.name) {}

bool OobeScreenId::operator==(const OobeScreenId& rhs) const {
  return name == rhs.name;
}

bool OobeScreenId::operator!=(const OobeScreenId& rhs) const {
  return name != rhs.name;
}

bool OobeScreenId::operator<(const OobeScreenId& rhs) const {
  return name < rhs.name;
}

std::ostream& operator<<(std::ostream& stream, const OobeScreenId& id) {
  return stream << id.name;
}

OobeScreenId StaticOobeScreenId::AsId() const {
  return OobeScreenId(name);
}

// static
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_HID_DETECTION;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_WELCOME;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_NETWORK;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_EULA;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_UPDATE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_ENROLLMENT;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_RESET;
constexpr StaticOobeScreenId OobeScreen::SCREEN_GAIA_SIGNIN;
constexpr StaticOobeScreenId OobeScreen::SCREEN_ACCOUNT_PICKER;
constexpr StaticOobeScreenId OobeScreen::SCREEN_KIOSK_AUTOLAUNCH;
constexpr StaticOobeScreenId OobeScreen::SCREEN_KIOSK_ENABLE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_ERROR_MESSAGE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_TPM_ERROR;
constexpr StaticOobeScreenId OobeScreen::SCREEN_PASSWORD_CHANGED;
constexpr StaticOobeScreenId
    OobeScreen::SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED;
constexpr StaticOobeScreenId OobeScreen::SCREEN_TERMS_OF_SERVICE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_WRONG_HWID;
constexpr StaticOobeScreenId OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK;
constexpr StaticOobeScreenId OobeScreen::SCREEN_APP_LAUNCH_SPLASH;
constexpr StaticOobeScreenId OobeScreen::SCREEN_ARC_KIOSK_SPLASH;
constexpr StaticOobeScreenId OobeScreen::SCREEN_CONFIRM_PASSWORD;
constexpr StaticOobeScreenId OobeScreen::SCREEN_FATAL_ERROR;
constexpr StaticOobeScreenId OobeScreen::SCREEN_DEVICE_DISABLED;
constexpr StaticOobeScreenId OobeScreen::SCREEN_USER_SELECTION;
constexpr StaticOobeScreenId
    OobeScreen::SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_ENCRYPTION_MIGRATION;
constexpr StaticOobeScreenId OobeScreen::SCREEN_SUPERVISION_TRANSITION;
constexpr StaticOobeScreenId OobeScreen::SCREEN_UPDATE_REQUIRED;
constexpr StaticOobeScreenId OobeScreen::SCREEN_ASSISTANT_OPTIN_FLOW;
constexpr StaticOobeScreenId OobeScreen::SCREEN_SPECIAL_LOGIN;
constexpr StaticOobeScreenId OobeScreen::SCREEN_SPECIAL_OOBE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_TEST_NO_WINDOW;
constexpr StaticOobeScreenId OobeScreen::SCREEN_SYNC_CONSENT;
constexpr StaticOobeScreenId OobeScreen::SCREEN_FINGERPRINT_SETUP;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_DEMO_SETUP;
constexpr StaticOobeScreenId OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES;
constexpr StaticOobeScreenId OobeScreen::SCREEN_RECOMMEND_APPS;
constexpr StaticOobeScreenId OobeScreen::SCREEN_APP_DOWNLOADING;
constexpr StaticOobeScreenId OobeScreen::SCREEN_DISCOVER;
constexpr StaticOobeScreenId OobeScreen::SCREEN_MARKETING_OPT_IN;
constexpr StaticOobeScreenId OobeScreen::SCREEN_MULTIDEVICE_SETUP;
constexpr StaticOobeScreenId OobeScreen::SCREEN_UNKNOWN;

}  // namespace chromeos
