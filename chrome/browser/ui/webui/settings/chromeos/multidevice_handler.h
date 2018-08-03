// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace settings {

// Chrome "Multidevice" (a.k.a. "Connected Devices") settings page UI handler.
class MultideviceHandler
    : public ::settings::SettingsPageUIHandler,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  explicit MultideviceHandler(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~MultideviceHandler() override;

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // ::settings::SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      multidevice_setup::mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) override;

  void HandleShowMultiDeviceSetupDialog(const base::ListValue* args);
  void HandleGetPageContent(const base::ListValue* args);
  void HandleRetryPendingHostSetup(const base::ListValue* args);

  void OnHostStatusFetched(
      const std::string& js_callback_id,
      multidevice_setup::mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device);

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  ScopedObserver<multidevice_setup::MultiDeviceSetupClient,
                 multidevice_setup::MultiDeviceSetupClient::Observer>
      multidevice_setup_observer_;

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<MultideviceHandler> callback_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MultideviceHandler);
};

}  // namespace settings

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
