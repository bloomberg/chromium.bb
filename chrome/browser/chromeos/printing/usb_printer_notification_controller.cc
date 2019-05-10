// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/usb_printer_notification_controller.h"

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_features.h"

namespace chromeos {

class UsbPrinterNotificationControllerImpl
    : public UsbPrinterNotificationController {
 public:
  explicit UsbPrinterNotificationControllerImpl(Profile* profile)
      : profile_(profile) {}
  ~UsbPrinterNotificationControllerImpl() override = default;

  void ShowEphemeralNotification(const Printer& printer) override {
    if (!base::FeatureList::IsEnabled(features::kStreamlinedUsbPrinterSetup)) {
      return;
    }

    if (base::ContainsKey(notifications_, printer.id())) {
      return;
    }

    notifications_[printer.id()] = std::make_unique<UsbPrinterNotification>(
        printer, GetUniqueNotificationId(),
        UsbPrinterNotification::Type::kEphemeral, profile_);
  }

  void RemoveNotification(const std::string& printer_id) override {
    if (!base::ContainsKey(notifications_, printer_id)) {
      return;
    }
    notifications_[printer_id]->CloseNotification();
    notifications_.erase(printer_id);
  }

  bool IsNotification(const std::string& printer_id) const override {
    return base::ContainsKey(notifications_, printer_id);
  }

 private:
  std::string GetUniqueNotificationId() {
    return base::StringPrintf("usb_printer_notification_%d",
                              next_notification_id_++);
  }

  std::map<std::string, std::unique_ptr<UsbPrinterNotification>> notifications_;
  Profile* profile_;
  int next_notification_id_ = 0;
};

std::unique_ptr<UsbPrinterNotificationController>
UsbPrinterNotificationController::Create(Profile* profile) {
  return std::make_unique<UsbPrinterNotificationControllerImpl>(profile);
}

}  // namespace chromeos
