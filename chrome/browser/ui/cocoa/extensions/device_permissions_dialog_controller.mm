// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/device_permissions_dialog_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/extensions/device_permissions_view_controller.h"
#include "device/usb/usb_device.h"

using extensions::DevicePermissionsPrompt;

DevicePermissionsDialogController::DevicePermissionsDialogController(
    content::WebContents* web_contents,
    DevicePermissionsPrompt::Delegate* delegate,
    scoped_refptr<DevicePermissionsPrompt::Prompt> prompt)
    : delegate_(delegate), prompt_(prompt) {
  view_controller_.reset(
      [[DevicePermissionsViewController alloc] initWithDelegate:this
                                                         prompt:prompt]);

  prompt_->SetObserver(this);

  base::scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[view_controller_ view] bounds]]);
  [[window contentView] addSubview:[view_controller_ view]];

  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, web_contents, sheet));
}

DevicePermissionsDialogController::~DevicePermissionsDialogController() {
  prompt_->SetObserver(nullptr);
}

void DevicePermissionsDialogController::OnUsbDevicesChosen(
    const std::vector<scoped_refptr<device::UsbDevice>>& devices) {
  delegate_->OnUsbDevicesChosen(devices);
  delegate_ = nullptr;
  constrained_window_->CloseWebContentsModalDialog();
}

void DevicePermissionsDialogController::OnDevicesChanged() {
  [view_controller_ devicesChanged];
}

void DevicePermissionsDialogController::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  if (delegate_) {
    std::vector<scoped_refptr<device::UsbDevice>> empty;
    delegate_->OnUsbDevicesChosen(empty);
  }
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void ChromeDevicePermissionsPrompt::ShowDialog() {
  // These objects will delete themselves when the dialog closes.
  new DevicePermissionsDialogController(web_contents(), delegate(), prompt());
}
