// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_DEBUG_WEBUI_PROXIMITY_AUTH_UI_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_DEBUG_WEBUI_PROXIMITY_AUTH_UI_H_

#include <string>

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace multidevice {

// The WebUI controller for chrome://proximity-auth.
class ProximityAuthUI : public ui::MojoWebUIController {
 public:
  using MultiDeviceSetupBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>)>;

  // Note: |web_ui| is not owned by this instance and must outlive this
  // instance.
  ProximityAuthUI(content::WebUI* web_ui,
                  device_sync::DeviceSyncClient* device_sync_client,
                  secure_channel::SecureChannelClient* secure_channel_client,
                  MultiDeviceSetupBinder multidevice_setup_binder);
  ~ProximityAuthUI() override;

  // Instantiates implementor of the mojom::MultiDeviceSetup mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
          receiver);

 private:
  const MultiDeviceSetupBinder multidevice_setup_binder_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthUI);
};

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_DEBUG_WEBUI_PROXIMITY_AUTH_UI_H_
