// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"

#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
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
    : ChooserController(owner), event_handler_(event_handler) {}

BluetoothChooserController::~BluetoothChooserController() {}

size_t BluetoothChooserController::NumOptions() const {
  return device_names_and_ids_.size();
}

const base::string16& BluetoothChooserController::GetOption(
    size_t index) const {
  DCHECK_LT(index, device_names_and_ids_.size());
  return device_names_and_ids_[index].first;
}

void BluetoothChooserController::Select(size_t index) {
  DCHECK_LT(index, device_names_and_ids_.size());
  event_handler_.Run(content::BluetoothChooser::Event::SELECTED,
                     device_names_and_ids_[index].second);
}

void BluetoothChooserController::Cancel() {
  event_handler_.Run(content::BluetoothChooser::Event::CANCELLED,
                     std::string());
}

void BluetoothChooserController::Close() {
  event_handler_.Run(content::BluetoothChooser::Event::CANCELLED,
                     std::string());
}

void BluetoothChooserController::OpenHelpCenterUrl() const {
  GetBrowser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChooserBluetoothOverviewURL), content::Referrer(),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initialized */));
}

void BluetoothChooserController::AddDevice(const std::string& device_id,
                                           const base::string16& device_name) {
  device_names_and_ids_.push_back(std::make_pair(device_name, device_id));
  if (observer())
    observer()->OnOptionAdded(device_names_and_ids_.size() - 1);
}

void BluetoothChooserController::RemoveDevice(const std::string& device_id) {
  for (auto it = device_names_and_ids_.begin();
       it != device_names_and_ids_.end(); ++it) {
    if (it->second == device_id) {
      size_t index = it - device_names_and_ids_.begin();
      device_names_and_ids_.erase(it);
      if (observer())
        observer()->OnOptionRemoved(index);
      return;
    }
  }
}
