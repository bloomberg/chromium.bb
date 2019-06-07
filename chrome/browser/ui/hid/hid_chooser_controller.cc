// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hid/hid_chooser_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

HidChooserController::HidChooserController(
    content::RenderFrameHost* render_frame_host,
    content::HidChooser::Callback callback)
    : ChooserController(render_frame_host,
                        IDS_HID_CHOOSER_PROMPT_ORIGIN,
                        IDS_HID_CHOOSER_PROMPT_EXTENSION_NAME),
      callback_(std::move(callback)),
      requesting_origin_(render_frame_host->GetLastCommittedOrigin()),
      embedding_origin_(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetMainFrame()
              ->GetLastCommittedOrigin()) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chooser_context_ =
      HidChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);

  chooser_context_->GetHidManager()->GetDevices(base::BindOnce(
      &HidChooserController::OnGotDevices, weak_factory_.GetWeakPtr()));

  // TODO(mattreynolds): Register to receive device added and removed events.
}

HidChooserController::~HidChooserController() {
  if (callback_)
    std::move(callback_).Run(nullptr);
}

bool HidChooserController::ShouldShowHelpButton() const {
  return false;
}

base::string16 HidChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 HidChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

size_t HidChooserController::NumOptions() const {
  return devices_.size();
}

base::string16 HidChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, devices_.size());
  const device::mojom::HidDeviceInfo& device = *devices_[index];
  if (device.product_name.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_HID_CHOOSER_ITEM_WITHOUT_NAME,
        base::ASCIIToUTF16(base::StringPrintf("0x%04x", device.vendor_id)),
        base::ASCIIToUTF16(base::StringPrintf("0x%04x", device.product_id)));
  }
  return l10n_util::GetStringFUTF16(
      IDS_HID_CHOOSER_ITEM_WITH_NAME, base::UTF8ToUTF16(device.product_name),
      base::ASCIIToUTF16(base::StringPrintf("0x%04x", device.vendor_id)),
      base::ASCIIToUTF16(base::StringPrintf("0x%04x", device.product_id)));
}

bool HidChooserController::IsPaired(size_t index) const {
  DCHECK_LE(index, devices_.size());

  if (!chooser_context_)
    return false;

  return chooser_context_->HasDevicePermission(
      requesting_origin_, embedding_origin_, *devices_[index]);
}

void HidChooserController::Select(const std::vector<size_t>& indices) {
  // TODO(crbug.com/964041): Record metrics when an item is selected.
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, devices_.size());

  if (!chooser_context_) {
    std::move(callback_).Run(nullptr);
    return;
  }

  chooser_context_->GrantDevicePermission(requesting_origin_, embedding_origin_,
                                          *devices_[index]);
  std::move(callback_).Run(std::move(devices_[index]));
}

void HidChooserController::Cancel() {
  // Called when the user presses the Cancel button in the chooser dialog.
  // TODO(crbug.com/964041): Record metrics when the chooser dialog is canceled.
}

void HidChooserController::Close() {
  // Called when the user dismisses the chooser by clicking outside the chooser
  // dialog, or when the dialog closes without the user taking action.
  // TODO(crbug.com/964041): Record metrics when the chooser dialog is closed.
}

void HidChooserController::OpenHelpCenterUrl() const {
  NOTIMPLEMENTED();
}

void HidChooserController::OnGotDevices(
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  for (auto& device : devices) {
    if (UsbBlocklist::Get().IsExcluded(
            {device->vendor_id, device->product_id, 0})) {
      continue;
    }
    // TODO(mattreynolds): Add |device| to |devices_| only if it matches the
    // filters in the requestDevice call.
    devices_.push_back(std::move(device));
  }

  if (view())
    view()->OnOptionsInitialized();
}
