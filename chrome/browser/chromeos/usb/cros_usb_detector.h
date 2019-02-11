// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_

#include <vector>

#include "base/macros.h"
#include "device/usb/public/mojom/device_enumeration_options.mojom.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "device/usb/public/mojom/device_manager_client.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace chromeos {

// Reasons the notification may be closed. These are used in histograms so do
// not remove/reorder entries. Only add at the end just before kMaxValue. Also
// remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum class CrosUsbNotificationClosed {
  // The notification was dismissed but not by the user (either automatically
  // or because the device was unplugged).
  kUnknown,
  // The user closed the notification via the close box.
  kByUser,
  // The user clicked on the Connect to Linux button of the notification.
  kConnectToLinux,
  // Maximum value for the enum.
  kMaxValue = kConnectToLinux
};

// Detects USB Devices for Chrome OS and manages UI for controlling their use
// with CrOS, Web or GuestOSs.
class CrosUsbDetector : public device::mojom::UsbDeviceManagerClient {
 public:
  // Used to namespace USB notifications to avoid clashes with WebUsbDetector.
  static std::string MakeNotificationId(const std::string& guid);

  CrosUsbDetector();
  ~CrosUsbDetector() override;

  void SetDeviceManagerForTesting(
      device::mojom::UsbDeviceManagerPtr device_manager);

  // Connect to the device manager to be notified of connection/removal.
  // Used during browser startup, after connection errors and to setup a fake
  // device manager during testing.
  void ConnectToDeviceManager();

 private:
  // Called after USB device access has been checked.
  void OnDeviceChecked(device::mojom::UsbDeviceInfoPtr device, bool allowed);

  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device) override;

  void OnDeviceManagerConnectionError();

  device::mojom::UsbDeviceManagerPtr device_manager_;
  mojo::AssociatedBinding<device::mojom::UsbDeviceManagerClient>
      client_binding_;

  std::vector<device::mojom::UsbDeviceFilterPtr> guest_os_classes_blocked_;
  std::vector<device::mojom::UsbDeviceFilterPtr>
      guest_os_classes_without_notif_;

  DISALLOW_COPY_AND_ASSIGN(CrosUsbDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
