// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/mock_chooser_controller.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

MockChooserController::MockChooserController(content::RenderFrameHost* owner)
    : ChooserController(owner,
                        IDS_USB_DEVICE_CHOOSER_PROMPT_ORIGIN,
                        IDS_USB_DEVICE_CHOOSER_PROMPT_EXTENSION_NAME),
      no_options_text_(l10n_util::GetStringUTF16(
          IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT)) {}

MockChooserController::~MockChooserController() {}

base::string16 MockChooserController::GetNoOptionsText() const {
  return no_options_text_;
}

base::string16 MockChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

size_t MockChooserController::NumOptions() const {
  return option_names_.size();
}

base::string16 MockChooserController::GetOption(size_t index) const {
  return option_names_[index];
}

base::string16 MockChooserController::GetStatus() const {
  return status_text_;
}

void MockChooserController::OnAdapterPresenceChanged(
    content::BluetoothChooser::AdapterPresence presence) {
  ClearAllOptions();
  switch (presence) {
    case content::BluetoothChooser::AdapterPresence::ABSENT:
      NOTREACHED();
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_OFF:
      no_options_text_ =
          l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_ADAPTER_OFF);
      status_text_ = base::string16();
      if (view())
        view()->OnAdapterEnabledChanged(false /* Adapter is turned off */);
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_ON:
      no_options_text_ =
          l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
      status_text_ =
          l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN);
      if (view())
        view()->OnAdapterEnabledChanged(true /* Adapter is turned on */);
      break;
  }
}

void MockChooserController::OnDiscoveryStateChanged(
    content::BluetoothChooser::DiscoveryState state) {
  switch (state) {
    case content::BluetoothChooser::DiscoveryState::DISCOVERING:
      ClearAllOptions();
      status_text_ =
          l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING);
      if (view()) {
        view()->OnRefreshStateChanged(
            true /* Refreshing options is in progress */);
      }
      break;
    case content::BluetoothChooser::DiscoveryState::IDLE:
    case content::BluetoothChooser::DiscoveryState::FAILED_TO_START:
      status_text_ =
          l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN);
      if (view()) {
        view()->OnRefreshStateChanged(
            false /* Refreshing options is complete */);
      }
      break;
  }
}

void MockChooserController::OptionAdded(const base::string16 option_name) {
  option_names_.push_back(option_name);
  if (view())
    view()->OnOptionAdded(option_names_.size() - 1);
}

void MockChooserController::OptionRemoved(const base::string16 option_name) {
  for (auto it = option_names_.begin(); it != option_names_.end(); ++it) {
    if (*it == option_name) {
      size_t index = it - option_names_.begin();
      option_names_.erase(it);
      if (view())
        view()->OnOptionRemoved(index);
      return;
    }
  }
}

void MockChooserController::ClearAllOptions() {
  option_names_.clear();
}
