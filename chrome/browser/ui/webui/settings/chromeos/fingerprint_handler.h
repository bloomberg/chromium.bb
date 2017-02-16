// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FINGERPRINT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FINGERPRINT_HANDLER_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {
namespace settings {

// Chrome OS fingerprint setup settings page UI handler.
class FingerprintHandler : public ::settings::SettingsPageUIHandler {
 public:
  FingerprintHandler();
  ~FingerprintHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleGetFingerprintsList(const base::ListValue* args);
  void HandleGetNumFingerprints(const base::ListValue* args);
  void HandleStartEnroll(const base::ListValue* args);
  void HandleCancelCurrentEnroll(const base::ListValue* args);
  void HandleGetEnrollmentLabel(const base::ListValue* args);
  void HandleRemoveEnrollment(const base::ListValue* args);
  void HandleChangeEnrollmentLabel(const base::ListValue* args);
  void HandleStartAuthentication(const base::ListValue* args);
  void HandleEndCurrentAuthentication(const base::ListValue* args);

  // TODO(sammiequon): Remove this when we can hook up to the fingerprint mojo
  // service. This is used to help manual testing in the meantime.
  void HandleFakeScanComplete(const base::ListValue* args);

  // TODO(sammiequon): Remove this when HandleFakeScanComplete is removed.
  std::vector<base::string16> fingerprints_list_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FINGERPRINT_HANDLER_H_
