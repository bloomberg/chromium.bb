// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FINGERPRINT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FINGERPRINT_HANDLER_H_

#include <unordered_map>

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/interfaces/fingerprint.mojom.h"

class Profile;

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {
namespace settings {

// Chrome OS fingerprint setup settings page UI handler.
class FingerprintHandler : public ::settings::SettingsPageUIHandler,
                           public device::mojom::FingerprintObserver,
                           public session_manager::SessionManagerObserver {
 public:
  explicit FingerprintHandler(Profile* profile);
  ~FingerprintHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  using AttemptMatches =
      std::unordered_map<std::string, std::vector<std::string>>;

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnEnrollScanDone(uint32_t scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;
  void OnAuthScanDone(uint32_t scan_result,
                      const AttemptMatches& matches) override;
  void OnSessionFailed() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  void HandleGetFingerprintsList(const base::ListValue* args);
  void HandleGetNumFingerprints(const base::ListValue* args);
  void HandleStartEnroll(const base::ListValue* args);
  void HandleCancelCurrentEnroll(const base::ListValue* args);
  void HandleGetEnrollmentLabel(const base::ListValue* args);
  void HandleRemoveEnrollment(const base::ListValue* args);
  void HandleChangeEnrollmentLabel(const base::ListValue* args);
  void HandleStartAuthentication(const base::ListValue* args);
  void HandleEndCurrentAuthentication(const base::ListValue* args);

  void OnGetFingerprintsList(const std::string& callback_id,
                             const std::unordered_map<std::string, std::string>&
                                 fingerprints_list_mapping);
  void OnRequestRecordLabel(const std::string& callback_id,
                            const std::string& label);
  void OnCancelCurrentEnrollSession(bool success);
  void OnRemoveRecord(const std::string& callback_id, bool success);
  void OnSetRecordLabel(const std::string& callback_id, bool success);
  void OnEndCurrentAuthSession(bool success);

  Profile* profile_;  // unowned

  std::vector<std::string> fingerprints_labels_;
  std::vector<std::string> fingerprints_paths_;
  std::string user_id_;

  device::mojom::FingerprintPtr fp_service_;
  mojo::Binding<device::mojom::FingerprintObserver> binding_;

  base::WeakPtrFactory<FingerprintHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FINGERPRINT_HANDLER_H_
