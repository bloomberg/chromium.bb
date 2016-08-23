// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

Browser* GetBrowser() {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetActiveUserProfile());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

}  // namespace

BluetoothChooserController::BluetoothChooserController(
    content::RenderFrameHost* owner,
    const content::BluetoothChooser::EventHandler& event_handler)
    : ChooserController(owner,
                        IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT_ORIGIN,
                        IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT_EXTENSION_NAME),
      event_handler_(event_handler),
      no_devices_text_(l10n_util::GetStringUTF16(
          IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT)) {}

BluetoothChooserController::~BluetoothChooserController() {}

bool BluetoothChooserController::ShouldShowIconBeforeText() const {
  return true;
}

base::string16 BluetoothChooserController::GetNoOptionsText() const {
  return no_devices_text_;
}

base::string16 BluetoothChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_PAIR_BUTTON_TEXT);
}

size_t BluetoothChooserController::NumOptions() const {
  return devices_.size();
}

int BluetoothChooserController::GetSignalStrengthLevel(size_t index) const {
  return devices_[index].signal_strength_level;
}

base::string16 BluetoothChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, devices_.size());
  const std::string& device_id = devices_[index].id;
  const auto& device_name_it = device_id_to_name_map_.find(device_id);
  DCHECK(device_name_it != device_id_to_name_map_.end());
  const auto& it = device_name_counts_.find(device_name_it->second);
  DCHECK(it != device_name_counts_.end());
  return it->second == 1
             ? device_name_it->second
             : l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_WITH_ID,
                   device_name_it->second, base::UTF8ToUTF16(device_id));
}

void BluetoothChooserController::RefreshOptions() {
  if (event_handler_.is_null())
    return;
  ClearAllDevices();
  event_handler_.Run(content::BluetoothChooser::Event::RESCAN, std::string());
}

base::string16 BluetoothChooserController::GetStatus() const {
  return status_text_;
}

void BluetoothChooserController::Select(size_t index) {
  if (event_handler_.is_null()) {
    content::RecordRequestDeviceOutcome(
        content::UMARequestDeviceOutcome::
            BLUETOOTH_CHOOSER_EVENT_HANDLER_INVALID);
    return;
  }
  DCHECK_LT(index, devices_.size());
  event_handler_.Run(content::BluetoothChooser::Event::SELECTED,
                     devices_[index].id);
}

void BluetoothChooserController::Cancel() {
  if (event_handler_.is_null())
    return;
  event_handler_.Run(content::BluetoothChooser::Event::CANCELLED,
                     std::string());
}

void BluetoothChooserController::Close() {
  if (event_handler_.is_null())
    return;
  event_handler_.Run(content::BluetoothChooser::Event::CANCELLED,
                     std::string());
}

void BluetoothChooserController::OpenHelpCenterUrl() const {
  GetBrowser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChooserBluetoothOverviewURL), content::Referrer(),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initialized */));
}

void BluetoothChooserController::OnAdapterPresenceChanged(
    content::BluetoothChooser::AdapterPresence presence) {
  ClearAllDevices();
  switch (presence) {
    case content::BluetoothChooser::AdapterPresence::ABSENT:
      NOTREACHED();
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_OFF:
      no_devices_text_ =
          l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_ADAPTER_OFF);
      status_text_ = base::string16();
      if (view()) {
        view()->OnAdapterEnabledChanged(
            false /* Bluetooth adapter is turned off */);
      }
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_ON:
      no_devices_text_ = l10n_util::GetStringUTF16(
          IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
      status_text_ =
          l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN);
      if (view()) {
        view()->OnAdapterEnabledChanged(
            true /* Bluetooth adapter is turned on */);
      }
      break;
  }
}

void BluetoothChooserController::OnDiscoveryStateChanged(
    content::BluetoothChooser::DiscoveryState state) {
  switch (state) {
    case content::BluetoothChooser::DiscoveryState::DISCOVERING:
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

void BluetoothChooserController::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const base::string16& device_name,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  auto name_it = device_id_to_name_map_.find(device_id);
  if (name_it != device_id_to_name_map_.end()) {
    if (should_update_name) {
      base::string16 previous_device_name = name_it->second;
      name_it->second = device_name;

      const auto& it = device_name_counts_.find(previous_device_name);
      DCHECK(it != device_name_counts_.end());
      DCHECK_GT(it->second, 0);

      if (--(it->second) == 0)
        device_name_counts_.erase(it);

      ++device_name_counts_[device_name];
    }

    auto device_it =
        std::find_if(devices_.begin(), devices_.end(),
                     [&device_id](const BluetoothDeviceInfo& device) {
                       return device.id == device_id;
                     });

    DCHECK(device_it != devices_.end());
    // http://crbug.com/543466 Update connection and paired status

    // When Bluetooth device scanning stops, the |signal_strength_level|
    // is -1, and in this case, should still use the previously stored
    // signal strength level value.
    if (signal_strength_level != -1)
      device_it->signal_strength_level = signal_strength_level;
    if (view())
      view()->OnOptionUpdated(device_it - devices_.begin());
    return;
  }

  devices_.push_back({device_id, signal_strength_level});
  device_id_to_name_map_.insert({device_id, device_name});
  ++device_name_counts_[device_name];
  if (view())
    view()->OnOptionAdded(devices_.size() - 1);
}

void BluetoothChooserController::RemoveDevice(const std::string& device_id) {
  const auto& name_it = device_id_to_name_map_.find(device_id);
  if (name_it == device_id_to_name_map_.end())
    return;

  auto device_it =
      std::find_if(devices_.begin(), devices_.end(),
                   [&device_id](const BluetoothDeviceInfo& device) {
                     return device.id == device_id;
                   });

  if (device_it != devices_.end()) {
    devices_.erase(device_it);

    const auto& it = device_name_counts_.find(name_it->second);
    DCHECK(it != device_name_counts_.end());
    DCHECK_GT(it->second, 0);

    if (--(it->second) == 0)
      device_name_counts_.erase(it);

    device_id_to_name_map_.erase(name_it);

    if (view())
      view()->OnOptionRemoved(device_it - devices_.begin());
  }
}

void BluetoothChooserController::ResetEventHandler() {
  event_handler_.Reset();
}

void BluetoothChooserController::ClearAllDevices() {
  devices_.clear();
  device_id_to_name_map_.clear();
  device_name_counts_.clear();
}
