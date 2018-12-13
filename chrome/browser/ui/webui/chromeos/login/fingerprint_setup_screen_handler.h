// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace chromeos {

class FingerprintSetupScreen;

// The sole implementation of the FingerprintSetupScreenView, using WebUI.
class FingerprintSetupScreenHandler
    : public BaseScreenHandler,
      public FingerprintSetupScreenView,
      public device::mojom::FingerprintObserver {
 public:
  FingerprintSetupScreenHandler();
  ~FingerprintSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // FingerprintSetupScreenView:
  void Bind(FingerprintSetupScreen* screen) override;
  void Show() override;
  void Hide() override;

  // BaseScreenHandler:
  void Initialize() override;

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;
  void OnAuthScanDone(
      device::mojom::ScanResult scan_result,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;

 private:
  // JS callbacks.
  void HandleStartEnroll(const base::ListValue* args);

  void OnCancelCurrentEnrollSession(bool success);

  FingerprintSetupScreen* screen_ = nullptr;

  device::mojom::FingerprintPtr fp_service_;
  mojo::Binding<device::mojom::FingerprintObserver> binding_{this};
  int enrolled_finger_count_ = 0;
  bool enroll_session_started_ = false;

  base::WeakPtrFactory<FingerprintSetupScreenHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FingerprintSetupScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
