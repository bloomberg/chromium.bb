// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_

#include <iosfwd>
#include <string>

namespace chromeos {

struct StaticOobeScreenId;

// Identifiers an OOBE screen.
struct OobeScreenId {
  // Create an identifier from a string.
  explicit OobeScreenId(const std::string& id);
  // Create an identifier from a statically created identifier. This is implicit
  // to make StaticOobeScreenId act more like OobeScreenId.
  OobeScreenId(const StaticOobeScreenId& id);

  // Name of the screen.
  std::string name;

  bool operator==(const OobeScreenId& rhs) const;
  bool operator!=(const OobeScreenId& rhs) const;
  bool operator<(const OobeScreenId& rhs) const;
  friend std::ostream& operator<<(std::ostream& stream, const OobeScreenId& id);
};

// A static identifier. An OOBE screen often statically expresses its ID in
// code. Chrome-style bans static destructors so use a const char* to point to
// the data in the binary instead of std::string.
struct StaticOobeScreenId {
  const char* name;

  OobeScreenId AsId() const;
};

struct OobeScreen {
  constexpr static StaticOobeScreenId SCREEN_OOBE_HID_DETECTION{
      "hid-detection"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_WELCOME{"connect"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_NETWORK{"network-selection"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_EULA{"eula"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_UPDATE{"update"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_ENABLE_DEBUGGING{"debugging"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_ENROLLMENT{
      "oauth-enrollment"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_RESET{"reset"};
  constexpr static StaticOobeScreenId SCREEN_GAIA_SIGNIN{"gaia-signin"};
  constexpr static StaticOobeScreenId SCREEN_ACCOUNT_PICKER{"account-picker"};
  constexpr static StaticOobeScreenId SCREEN_KIOSK_AUTOLAUNCH{"autolaunch"};
  constexpr static StaticOobeScreenId SCREEN_KIOSK_ENABLE{"kiosk-enable"};
  constexpr static StaticOobeScreenId SCREEN_ERROR_MESSAGE{"error-message"};
  constexpr static StaticOobeScreenId SCREEN_TPM_ERROR{"tpm-error-message"};
  constexpr static StaticOobeScreenId SCREEN_PASSWORD_CHANGED{
      "password-changed"};
  constexpr static StaticOobeScreenId
      SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED{"supervised-user-creation"};
  constexpr static StaticOobeScreenId SCREEN_TERMS_OF_SERVICE{
      "terms-of-service"};
  constexpr static StaticOobeScreenId SCREEN_ARC_TERMS_OF_SERVICE{"arc-tos"};
  constexpr static StaticOobeScreenId SCREEN_WRONG_HWID{"wrong-hwid"};
  constexpr static StaticOobeScreenId SCREEN_AUTO_ENROLLMENT_CHECK{
      "auto-enrollment-check"};
  constexpr static StaticOobeScreenId SCREEN_APP_LAUNCH_SPLASH{
      "app-launch-splash"};
  constexpr static StaticOobeScreenId SCREEN_ARC_KIOSK_SPLASH{
      "arc-kiosk-splash"};
  constexpr static StaticOobeScreenId SCREEN_CONFIRM_PASSWORD{
      "confirm-password"};
  constexpr static StaticOobeScreenId SCREEN_FATAL_ERROR{"fatal-error"};
  constexpr static StaticOobeScreenId SCREEN_DEVICE_DISABLED{"device-disabled"};
  constexpr static StaticOobeScreenId SCREEN_USER_SELECTION{"userBoard"};
  constexpr static StaticOobeScreenId SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE{
      "ad-password-change"};
  constexpr static StaticOobeScreenId SCREEN_ENCRYPTION_MIGRATION{
      "encryption-migration"};
  constexpr static StaticOobeScreenId SCREEN_SUPERVISION_TRANSITION{
      "supervision-transition"};
  constexpr static StaticOobeScreenId SCREEN_UPDATE_REQUIRED{"update-required"};
  constexpr static StaticOobeScreenId SCREEN_ASSISTANT_OPTIN_FLOW{
      "assistant-optin-flow"};

  // Special "first screen" that initiates login flow.
  constexpr static StaticOobeScreenId SCREEN_SPECIAL_LOGIN{"login"};
  // Special "first screen" that initiates full OOBE flow.
  constexpr static StaticOobeScreenId SCREEN_SPECIAL_OOBE{"oobe"};
  // Special test value that commands not to create any window yet.
  constexpr static StaticOobeScreenId SCREEN_TEST_NO_WINDOW{"test:nowindow"};

  constexpr static StaticOobeScreenId SCREEN_SYNC_CONSENT{"sync-consent"};
  constexpr static StaticOobeScreenId SCREEN_FINGERPRINT_SETUP{
      "fingerprint-setup"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_DEMO_SETUP{"demo-setup"};
  constexpr static StaticOobeScreenId SCREEN_OOBE_DEMO_PREFERENCES{
      "demo-preferences"};
  constexpr static StaticOobeScreenId SCREEN_RECOMMEND_APPS{"recommend-apps"};
  constexpr static StaticOobeScreenId SCREEN_APP_DOWNLOADING{"app-downloading"};
  constexpr static StaticOobeScreenId SCREEN_DISCOVER{"discover"};
  constexpr static StaticOobeScreenId SCREEN_MARKETING_OPT_IN{
      "marketing-opt-in"};
  constexpr static StaticOobeScreenId SCREEN_MULTIDEVICE_SETUP{
      "multidevice-setup"};
  constexpr static StaticOobeScreenId SCREEN_UNKNOWN{"unknown"};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_
