// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/usb/cros_usb_detector.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/service_manager_connection.h"
#include "device/usb/public/cpp/usb_utils.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

const char kNotifierUsb[] = "crosusb.connected";

void RecordNotificationClosure(CrosUsbNotificationClosed disposition) {
  UMA_HISTOGRAM_ENUMERATION("CrosUsb.NotificationClosed", disposition);
}

// Delegate for CrOSUsb notification
class UsbNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit UsbNotificationDelegate(const std::string& notification_id,
                                   const std::string& device_id)
      : notification_id_(notification_id),
        device_id_(device_id),
        disposition_(CrosUsbNotificationClosed::kUnknown) {}

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    disposition_ = CrosUsbNotificationClosed::kConnectToLinux;
    // TODO(jopra): add device_id_ to shared USBs list (instead of opening
    // settings page)
    chrome::ShowSettingsSubPageForProfile(
        ProfileManager::GetActiveUserProfile(),
        chrome::kCrostiniSharedUsbDevicesSubPage);
    Close(false);
  }

  void Close(bool by_user) override {
    if (by_user)
      disposition_ = chromeos::CrosUsbNotificationClosed::kByUser;
    RecordNotificationClosure(disposition_);
  }

 private:
  ~UsbNotificationDelegate() override = default;

  std::string notification_id_;
  std::string device_id_;
  CrosUsbNotificationClosed disposition_;

  DISALLOW_COPY_AND_ASSIGN(UsbNotificationDelegate);
};

// List of class codes to handle / not handle.
// See https://www.usb.org/defined-class-codes for more information.
enum UsbClassCode : uint8_t {
  USB_CLASS_PER_INTERFACE = 0x00,
  USB_CLASS_AUDIO = 0x01,
  USB_CLASS_COMM = 0x02,
  USB_CLASS_HID = 0x03,
  USB_CLASS_PHYSICAL = 0x05,
  USB_CLASS_STILL_IMAGE = 0x06,
  USB_CLASS_PRINTER = 0x07,
  USB_CLASS_MASS_STORAGE = 0x08,
  USB_CLASS_HUB = 0x09,
  USB_CLASS_CDC_DATA = 0x0a,
  USB_CLASS_CSCID = 0x0b,
  USB_CLASS_CONTENT_SEC = 0x0d,
  USB_CLASS_VIDEO = 0x0e,
  USB_CLASS_PERSONAL_HEALTHCARE = 0x0f,
  USB_CLASS_BILLBOARD = 0x11,
  USB_CLASS_DIAGNOSTIC_DEVICE = 0xdc,
  USB_CLASS_WIRELESS_CONTROLLER = 0xe0,
  USB_CLASS_MISC = 0xef,
  USB_CLASS_APP_SPEC = 0xfe,
  USB_CLASS_VENDOR_SPEC = 0xff,
};

device::mojom::UsbDeviceFilterPtr UsbFilterByClassCode(
    UsbClassCode device_class) {
  auto filter = device::mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = device_class;
  return filter;
}

void ShowNotificationForDevice(device::mojom::UsbDeviceInfoPtr device_info) {
  base::string16 product_name =
      l10n_util::GetStringUTF16(IDS_CROSUSB_UNKNOWN_DEVICE);
  if (device_info->product_name.has_value() &&
      !device_info->product_name->empty()) {
    product_name = device_info->product_name.value();
  } else if (device_info->manufacturer_name.has_value() &&
             !device_info->manufacturer_name->empty()) {
    product_name =
        l10n_util::GetStringFUTF16(IDS_CROSUSB_UNKNOWN_DEVICE_FROM_MANUFACTURER,
                                   device_info->manufacturer_name.value());
  }

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_LINUX)));

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device_info->guid);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_MULTIPLE, notification_id,
      l10n_util::GetStringFUTF16(IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION_TITLE,
                                 product_name),
      l10n_util::GetStringUTF16(IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION),
      gfx::Image(), base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierUsb),
      rich_notification_data,
      base::MakeRefCounted<UsbNotificationDelegate>(notification_id,
                                                    device_info->guid));
  notification.SetSystemPriority();
  SystemNotificationHelper::GetInstance()->Display(notification);
}

}  // namespace

std::string CrosUsbDetector::MakeNotificationId(const std::string& guid) {
  return "cros:" + guid;
}

CrosUsbDetector::CrosUsbDetector() : client_binding_(this) {
  guest_os_classes_blocked_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PHYSICAL));
  guest_os_classes_blocked_.emplace_back(UsbFilterByClassCode(USB_CLASS_HID));
  guest_os_classes_blocked_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PRINTER));

  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_AUDIO));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_STILL_IMAGE));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_MASS_STORAGE));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_VIDEO));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PERSONAL_HEALTHCARE));
}

CrosUsbDetector::~CrosUsbDetector() {}

void CrosUsbDetector::SetDeviceManagerForTesting(
    device::mojom::UsbDeviceManagerPtr device_manager) {
  device_manager_ = std::move(device_manager);
}

void CrosUsbDetector::ConnectToDeviceManager() {
  // Tests may set a fake manager.
  if (!device_manager_) {
    // Request UsbDeviceManagerPtr from DeviceService.
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(device::mojom::kServiceName,
                        mojo::MakeRequest(&device_manager_));
  }
  DCHECK(device_manager_);
  device_manager_.set_connection_error_handler(
      base::BindOnce(&CrosUsbDetector::OnDeviceManagerConnectionError,
                     base::Unretained(this)));

  // Listen for added/removed device events.
  DCHECK(!client_binding_);
  device::mojom::UsbDeviceManagerClientAssociatedPtrInfo client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  device_manager_->SetClient(std::move(client));
}

void CrosUsbDetector::OnDeviceChecked(
    device::mojom::UsbDeviceInfoPtr device_info,
    bool allowed) {
  if (!allowed) {
    LOG(WARNING) << "Device not allowed by Permission Broker. product:"
                 << device_info->product_id
                 << " vendor:" << device_info->vendor_id;
    return;
  }

  // TODO(jopra): Enable device connection management for allowed, non-blocked
  // devices.

  // Some devices should not trigger the notification.
  if (device::UsbDeviceFilterMatchesAny(guest_os_classes_without_notif_,
                                        *device_info)) {
    return;
  }
  ShowNotificationForDevice(std::move(device_info));
}

void CrosUsbDetector::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  if (device::UsbDeviceFilterMatchesAny(guest_os_classes_blocked_,
                                        *device_info)) {
    return;  // Guest OS does not handle this kind of device.
  }
  device_manager_->CheckAccess(
      device_info->guid,
      base::BindOnce(&CrosUsbDetector::OnDeviceChecked, base::Unretained(this),
                     std::move(device_info)));
}

void CrosUsbDetector::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  SystemNotificationHelper::GetInstance()->Close(
      CrosUsbDetector::MakeNotificationId(device_info->guid));
}

void CrosUsbDetector::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_binding_.Close();

  ConnectToDeviceManager();
}

}  // namespace chromeos
