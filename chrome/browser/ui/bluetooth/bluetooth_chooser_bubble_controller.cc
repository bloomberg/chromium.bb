// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_bubble_controller.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "chrome/common/url_constants.h"
#include "components/bubble/bubble_controller.h"
#include "url/gurl.h"

BluetoothChooserBubbleController::BluetoothChooserBubbleController(
    content::RenderFrameHost* owner)
    : ChooserBubbleController(owner), bluetooth_chooser_(nullptr) {}

BluetoothChooserBubbleController::~BluetoothChooserBubbleController() {
  if (bluetooth_chooser_)
    bluetooth_chooser_->set_bluetooth_chooser_bubble_controller(nullptr);
}

size_t BluetoothChooserBubbleController::NumOptions() const {
  return device_names_and_ids_.size();
}

const base::string16& BluetoothChooserBubbleController::GetOption(
    size_t index) const {
  DCHECK_LT(index, device_names_and_ids_.size());
  return device_names_and_ids_[index].first;
}

void BluetoothChooserBubbleController::Select(size_t index) {
  DCHECK_LT(index, device_names_and_ids_.size());
  if (bluetooth_chooser_) {
    bluetooth_chooser_->CallEventHandler(
        content::BluetoothChooser::Event::SELECTED,
        device_names_and_ids_[index].second);
  }

  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
}

void BluetoothChooserBubbleController::Cancel() {
  if (bluetooth_chooser_) {
    bluetooth_chooser_->CallEventHandler(
        content::BluetoothChooser::Event::CANCELLED, std::string());
  }

  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_CANCELED);
}

void BluetoothChooserBubbleController::Close() {
  if (bluetooth_chooser_) {
    bluetooth_chooser_->CallEventHandler(
        content::BluetoothChooser::Event::CANCELLED, std::string());
  }
}

GURL BluetoothChooserBubbleController::GetHelpCenterUrl() const {
  return GURL(chrome::kChooserBluetoothOverviewURL);
}

void BluetoothChooserBubbleController::AddDevice(
    const std::string& device_id,
    const base::string16& device_name) {
  device_names_and_ids_.push_back(std::make_pair(device_name, device_id));
  if (observer())
    observer()->OnOptionAdded(device_names_and_ids_.size() - 1);
}

void BluetoothChooserBubbleController::RemoveDevice(
    const std::string& device_id) {
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
