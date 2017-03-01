// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printer_detector/printer_detector.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "device/base/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {
namespace {

const char kUSBPrinterFoundNotificationID[] =
    "chrome://settings/cupsPrinters/printer_found";

// Base class used for printer USB interfaces
// (https://www.usb.org/developers/defined_class).
const uint8_t kPrinterInterfaceClass = 7;

// USBPrinterSetupNotificationDelegate takes a pointer to the Impl class, so
// we have to forward declare it.
class CupsPrinterDetectorImpl;

class USBPrinterSetupNotificationDelegate : public NotificationDelegate {
 public:
  explicit USBPrinterSetupNotificationDelegate(
      CupsPrinterDetectorImpl* printer_detector)
      : printer_detector_(printer_detector) {}

  // NotificationDelegate override:
  std::string id() const override { return kUSBPrinterFoundNotificationID; }

  // This is defined out of line because it needs the PrinterDetectorImpl
  // full class declaration, not just the forward declaration.
  void ButtonClick(int button_index) override;

  bool HasClickedListener() override { return true; }

 private:
  ~USBPrinterSetupNotificationDelegate() override = default;

  CupsPrinterDetectorImpl* printer_detector_;

  DISALLOW_COPY_AND_ASSIGN(USBPrinterSetupNotificationDelegate);
};

// The PrinterDetector that drives the flow for setting up a USB printer to use
// CUPS backend.
class CupsPrinterDetectorImpl : public PrinterDetector,
                                public device::UsbService::Observer {
 public:
  explicit CupsPrinterDetectorImpl(Profile* profile)
      : profile_(profile), observer_(this), weak_ptr_factory_(this) {
    extensions::ExtensionSystem::Get(profile)->ready().Post(
        FROM_HERE, base::Bind(&CupsPrinterDetectorImpl::Initialize,
                              weak_ptr_factory_.GetWeakPtr()));
  }
  ~CupsPrinterDetectorImpl() override = default;

  void ClickOnNotificationButton(int button_index) {
    // Remove the old notification first.
    const ProfileID profile_id = NotificationUIManager::GetProfileID(profile_);
    g_browser_process->notification_ui_manager()->CancelById(
        kUSBPrinterFoundNotificationID, profile_id);

    if (command_ == ButtonCommand::SETUP) {
      OnSetUpUSBPrinterStarted();
      // TODO(skau/xdai): call the CUPS backend to set up the USB printer and
      // then call OnSetUpPrinterDone() or OnSetUpPrinterError() depending on
      // the setup result.
    } else if (command_ == ButtonCommand::CANCEL_SETUP) {
      // TODO(skau/xdai): call the CUPS backend to cancel the printer setup.
    } else if (command_ == ButtonCommand::GET_HELP) {
      chrome::NavigateParams params(profile_,
                                    GURL(chrome::kChromeUIMdCupsSettingsURL),
                                    ui::PAGE_TRANSITION_LINK);
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      params.window_action = chrome::NavigateParams::SHOW_WINDOW;
      chrome::Navigate(&params);
    }
  }

 private:
  // Action that should be performed when a notification button is clicked.
  enum class ButtonCommand {
    SETUP,
    CANCEL_SETUP,
    CLOSE,
    GET_HELP,
  };

  // UsbService::observer override:
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override {
    const user_manager::User* user =
        ProfileHelper::Get()->GetUserByProfile(profile_);
    // TODO(justincarlson) - See if it's appropriate to relax any of these
    // constraints.
    if (!user || !user->HasGaiaAccount() || !user_manager::UserManager::Get() ||
        user != user_manager::UserManager::Get()->GetActiveUser()) {
      return;
    }

    device::UsbDeviceFilter printer_filter;
    printer_filter.interface_class = kPrinterInterfaceClass;
    if (!printer_filter.Matches(device))
      return;

    ShowUSBPrinterSetupNotification(device);
  }

  // Initializes the printer detector.
  void Initialize() {
    device::UsbService* usb_service =
        device::DeviceClient::Get()->GetUsbService();
    if (!usb_service)
      return;
    observer_.Add(usb_service);
  }

  void SetNotificationUIManagerForTesting(
      NotificationUIManager* manager) override {
    LOG(FATAL) << "Not implemented for CUPS";
  }

  void ShowUSBPrinterSetupNotification(
      scoped_refptr<device::UsbDevice> device) {
    // TODO(justincarlson) - Test this notification across a wide variety of
    // less-than-sane printers to make sure the notification text stays as
    // intelligible as possible.
    base::string16 printer_name = device->manufacturer_string() +
                                  base::UTF8ToUTF16(" ") +
                                  device->product_string();
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    message_center::RichNotificationData data;
    data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_BUTTON)));
    notification_.reset(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        l10n_util::GetStringUTF16(
            IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_TITLE),      // title
        printer_name,                                             // body
        bundle.GetImageNamed(IDR_PRINTER_DETECTED_NOTIFICATION),  // icon
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   kUSBPrinterFoundNotificationID),
        base::string16(),  // display_source
        GURL(), kUSBPrinterFoundNotificationID, data,
        new USBPrinterSetupNotificationDelegate(this)));
    notification_->SetSystemPriority();
    command_ = ButtonCommand::SETUP;

    g_browser_process->notification_ui_manager()->Add(*notification_, profile_);
  }

  void OnSetUpUSBPrinterStarted() {
    notification_->set_title(l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_IN_PROGRESS_TITLE));
    notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
    notification_->set_progress(-1);
    std::vector<message_center::ButtonInfo> buttons;
    buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_CANCEL_BUTTON)));
    notification_->set_buttons(buttons);
    command_ = ButtonCommand::CANCEL_SETUP;
    g_browser_process->notification_ui_manager()->Add(*notification_, profile_);
  }

  void OnSetUpUSBPrinterDone() {
    notification_->set_title(l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_SUCCESS_TITLE));
    notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
    std::vector<message_center::ButtonInfo> buttons;
    buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_CLOSE_BUTTON)));
    notification_->set_buttons(buttons);
    command_ = ButtonCommand::CLOSE;
    g_browser_process->notification_ui_manager()->Add(*notification_, profile_);
  }

  void OnSetUpUSBPrinterError() {
    notification_->set_title(l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_FAILED_TITLE));
    notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
    std::vector<message_center::ButtonInfo> buttons;
    buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_SET_UP_GET_HELP_BUTTON)));
    notification_->set_buttons(buttons);
    command_ = ButtonCommand::GET_HELP;
    g_browser_process->notification_ui_manager()->Add(*notification_, profile_);
  }

  std::unique_ptr<Notification> notification_;
  ButtonCommand command_ = ButtonCommand::SETUP;

  Profile* profile_;
  ScopedObserver<device::UsbService, device::UsbService::Observer> observer_;
  base::WeakPtrFactory<CupsPrinterDetectorImpl> weak_ptr_factory_;
};

void USBPrinterSetupNotificationDelegate::ButtonClick(int button_index) {
  printer_detector_->ClickOnNotificationButton(button_index);
}

}  // namespace

// static
std::unique_ptr<PrinterDetector> PrinterDetector::CreateCups(Profile* profile) {
  return base::MakeUnique<CupsPrinterDetectorImpl>(profile);
}

}  // namespace chromeos
