// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printer_detector/printer_detector.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/extensions/api/webstore_widget_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_ids.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/one_shot_event.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace webstore_widget_private_api =
    extensions::api::webstore_widget_private;

namespace {

const char kPrinterProviderFoundNotificationID[] =
    "chrome://settings/printer/printer_app_found";

const char kNoPrinterProviderNotificationID[] =
    "chrome://settings/printer/no_printer_app";

// Base class used for printer USB interfaces
// (https://www.usb.org/developers/defined_class).
const uint8 kPrinterInterfaceClass = 7;

enum PrinterServiceEvent {
  PRINTER_ADDED,
  DEPRECATED_PAGE_DISPLAYED,
  NOTIFICATION_SHOWN_PRINTER_SUPPORTED,
  NOTIFICATION_SHOWN_PRINTER_NOT_SUPPORTED,
  WEBSTORE_WIDGET_APP_LAUNCHED,
  PRINTER_SERVICE_EVENT_MAX,
};

base::string16 GetNotificationTitle(uint16 vendor_id, uint16 product_id) {
  const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
  if (vendor_name) {
    return l10n_util::GetStringFUTF16(IDS_PRINTER_DETECTED_NOTIFICATION_TITLE,
                                      base::UTF8ToUTF16(vendor_name));
  } else {
    return l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_TITLE_UNKNOWN_VENDOR);
  }
}

std::string GetNotificationTag(const std::string& vendor_id,
                               const std::string& product_id) {
  return vendor_id + ":" + product_id;
}

// Checks if there is an enabled extension with printerProvider permission and
// usbDevices persmission for the USB (vendor_id, product_id) pair.
bool HasAppThatSupportsPrinter(Profile* profile,
                               const scoped_refptr<device::UsbDevice>& device) {
  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (const auto& extension : enabled_extensions) {
    if (!extension->permissions_data() ||
        !extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kPrinterProvider) ||
        !extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kUsb)) {
      continue;
    }

    const extensions::UsbPrinterManifestData* manifest_data =
        extensions::UsbPrinterManifestData::Get(extension.get());
    if (manifest_data && manifest_data->SupportsDevice(device)) {
      return true;
    }

    extensions::UsbDevicePermission::CheckParam param(
        device->vendor_id(), device->product_id(),
        extensions::UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
    if (extension->permissions_data()->CheckAPIPermissionWithParam(
            extensions::APIPermission::kUsbDevice, &param)) {
      return true;
    }
  }
  return false;
}

// Delegate for notification shown when a printer provider app for the plugged
// in printer is found.
class PrinterProviderExistsNotificationDelegate : public NotificationDelegate {
 public:
  PrinterProviderExistsNotificationDelegate(const std::string& vendor_id,
                                            const std::string& product_id)
      : vendor_id_(vendor_id), product_id_(product_id) {}

  std::string id() const override {
    return "system.printer.printer_provider_exists/" +
           GetNotificationTag(vendor_id_, product_id_);
  }

 private:
  ~PrinterProviderExistsNotificationDelegate() override = default;

  const std::string vendor_id_;
  const std::string product_id_;

  DISALLOW_COPY_AND_ASSIGN(PrinterProviderExistsNotificationDelegate);
};

// Delegate for notification shown when there are no printer provider apps that
// support the plugged in printer found.
// The notification is clickable, and clicking it is supposed to launch
// Chrome Web Store widget listing apps that can support the plugged in printer.
// (not implemented yet).
class SearchPrinterAppNotificationDelegate : public NotificationDelegate {
 public:
  SearchPrinterAppNotificationDelegate(content::BrowserContext* browser_context,
                                       uint16 vendor_id,
                                       const std::string& vendor_id_str,
                                       uint16 product_id,
                                       const std::string& product_id_str)
      : browser_context_(browser_context),
        vendor_id_(vendor_id),
        vendor_id_str_(vendor_id_str),
        product_id_(product_id),
        product_id_str_(product_id_str) {}

  std::string id() const override {
    return "system.printer.no_printer_provider_found/" +
           GetNotificationTag(vendor_id_str_, product_id_str_);
  }
  bool HasClickedListener() override { return true; }

  void Click() override {
    UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent",
                              WEBSTORE_WIDGET_APP_LAUNCHED,
                              PRINTER_SERVICE_EVENT_MAX);
    webstore_widget_private_api::Options options;
    options.type = webstore_widget_private_api::TYPE_PRINTER_PROVIDER;
    options.usb_id.reset(new webstore_widget_private_api::UsbId());
    options.usb_id->vendor_id = vendor_id_;
    options.usb_id->product_id = product_id_;

    extensions::EventRouter* event_router =
        extensions::EventRouter::Get(browser_context_);
    scoped_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::WEBSTORE_WIDGET_PRIVATE_ON_SHOW_WIDGET,
        webstore_widget_private_api::OnShowWidget::kEventName,
        webstore_widget_private_api::OnShowWidget::Create(options)));
    event_router->DispatchEventToExtension(extension_misc::kWebstoreWidgetAppId,
                                           event.Pass());
  }

 private:
  ~SearchPrinterAppNotificationDelegate() override = default;

  content::BrowserContext* browser_context_;
  uint16 vendor_id_;
  std::string vendor_id_str_;
  uint16 product_id_;
  std::string product_id_str_;

  DISALLOW_COPY_AND_ASSIGN(SearchPrinterAppNotificationDelegate);
};

// Shows a notification for a plugged in printer.
// If there is a printerProvider app that handles the printer's USB (vendor_id,
// product_id) pair, the notification informs the user that the printer is ready
// to be used, otherwise it offers the user to search the Chrome Web Store for
// an app that can handle the printer.
void ShowPrinterPluggedNotification(
    Profile* profile,
    NotificationUIManager* notification_ui_manager,
    const scoped_refptr<device::UsbDevice>& device) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification;

  const std::string kVendorIdStr = base::IntToString(device->vendor_id());
  const std::string kProductIdStr = base::IntToString(device->product_id());

  if (HasAppThatSupportsPrinter(profile, device)) {
    UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent",
                              NOTIFICATION_SHOWN_PRINTER_SUPPORTED,
                              PRINTER_SERVICE_EVENT_MAX);
    notification.reset(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GetNotificationTitle(device->vendor_id(), device->product_id()),
        l10n_util::GetStringUTF16(
            IDS_PRINTER_DETECTED_NOTIFICATION_PRINT_APP_FOUND_BODY),
        bundle.GetImageNamed(IDR_PRINTER_NOTIFICATION),
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   kPrinterProviderFoundNotificationID),
        base::string16(), GURL(kPrinterProviderFoundNotificationID),
        GetNotificationTag(kVendorIdStr, kProductIdStr),
        message_center::RichNotificationData(),
        new PrinterProviderExistsNotificationDelegate(kVendorIdStr,
                                                      kProductIdStr)));
  } else {
    UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent",
                              NOTIFICATION_SHOWN_PRINTER_NOT_SUPPORTED,
                              PRINTER_SERVICE_EVENT_MAX);
    message_center::RichNotificationData options;
    options.clickable = true;
    notification.reset(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GetNotificationTitle(device->vendor_id(), device->product_id()),
        l10n_util::GetStringUTF16(
            IDS_PRINTER_DETECTED_NOTIFICATION_NO_PRINT_APP_BODY),
        bundle.GetImageNamed(IDR_PRINTER_NOTIFICATION),
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   kNoPrinterProviderNotificationID),
        base::string16(), GURL(kNoPrinterProviderNotificationID),
        GetNotificationTag(kVendorIdStr, kProductIdStr), options,
        new SearchPrinterAppNotificationDelegate(
            profile, device->vendor_id(), kVendorIdStr, device->product_id(),
            kProductIdStr)));
  }

  notification->SetSystemPriority();
  notification_ui_manager->Add(*notification, profile);
}

}  // namespace

namespace chromeos {

PrinterDetector::PrinterDetector(Profile* profile)
    : profile_(profile),
      notification_ui_manager_(nullptr),
      observer_(this),
      weak_ptr_factory_(this) {
  extensions::ExtensionSystem::Get(profile)->ready().Post(
      FROM_HERE,
      base::Bind(&PrinterDetector::Initialize, weak_ptr_factory_.GetWeakPtr()));
}

PrinterDetector::~PrinterDetector() {
}

void PrinterDetector::Shutdown() {
}

void PrinterDetector::Initialize() {
  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  if (!usb_service)
    return;
  observer_.Add(usb_service);
}

void PrinterDetector::OnDeviceAdded(scoped_refptr<device::UsbDevice> device) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  if (!user || !user->HasGaiaAccount() || !user_manager::UserManager::Get() ||
      user != user_manager::UserManager::Get()->GetActiveUser()) {
    return;
  }

  device::UsbDeviceFilter printer_filter;
  printer_filter.SetInterfaceClass(kPrinterInterfaceClass);
  if (!printer_filter.Matches(device)) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent", PRINTER_ADDED,
                            PRINTER_SERVICE_EVENT_MAX);

  ShowPrinterPluggedNotification(
      profile_,
      notification_ui_manager_ ? notification_ui_manager_
                               : g_browser_process->notification_ui_manager(),
      device);
}

void PrinterDetector::SetNotificationUIManagerForTesting(
    NotificationUIManager* manager) {
  notification_ui_manager_ = manager;
}

}  // namespace chromeos
