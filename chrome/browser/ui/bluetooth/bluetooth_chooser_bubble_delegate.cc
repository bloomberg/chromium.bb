// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_bubble_delegate.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "components/bubble/bubble_controller.h"

BluetoothChooserBubbleDelegate::BluetoothChooserBubbleDelegate(Browser* browser)
    : ChooserBubbleDelegate(browser), bluetooth_chooser_(nullptr) {}

BluetoothChooserBubbleDelegate::~BluetoothChooserBubbleDelegate() {
  if (bluetooth_chooser_)
    bluetooth_chooser_->set_bluetooth_chooser_bubble_delegate(nullptr);
}

const std::vector<base::string16>& BluetoothChooserBubbleDelegate::GetOptions()
    const {
  return device_names_;
}

// TODO(juncai): Change the index type to be size_t in base class to avoid
// extra type casting.
void BluetoothChooserBubbleDelegate::Select(int index) {
  size_t idx = static_cast<size_t>(index);
  size_t num_options = device_ids_.size();
  DCHECK_LT(idx, num_options);
  if (bluetooth_chooser_) {
    bluetooth_chooser_->CallEventHandler(
        content::BluetoothChooser::Event::SELECTED, device_ids_[idx]);
  }

  if (bubble_controller_)
    bubble_controller_->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
}

void BluetoothChooserBubbleDelegate::Cancel() {
  if (bluetooth_chooser_) {
    bluetooth_chooser_->CallEventHandler(
        content::BluetoothChooser::Event::CANCELLED, std::string());
  }

  if (bubble_controller_)
    bubble_controller_->CloseBubble(BUBBLE_CLOSE_CANCELED);
}

void BluetoothChooserBubbleDelegate::Close() {
  if (bluetooth_chooser_) {
    bluetooth_chooser_->CallEventHandler(
        content::BluetoothChooser::Event::CANCELLED, std::string());
  }
}

void BluetoothChooserBubbleDelegate::AddDevice(
    const std::string& device_id,
    const base::string16& device_name) {
  DCHECK(!ContainsValue(device_ids_, device_id));
  device_names_.push_back(device_name);
  device_ids_.push_back(device_id);
  // TODO(juncai): Change OnOptionAdded's index type to be size_t to avoid
  // extra type casting here.
  if (observer())
    observer()->OnOptionAdded(static_cast<int>(device_names_.size()) - 1);
}

void BluetoothChooserBubbleDelegate::RemoveDevice(
    const std::string& device_id) {
  auto iter = std::find(device_ids_.begin(), device_ids_.end(), device_id);
  if (iter != device_ids_.end()) {
    size_t index = iter - device_ids_.begin();
    device_ids_.erase(iter);
    device_names_.erase(device_names_.begin() + index);
    // TODO(juncai): Change OnOptionRemoved's index type to be size_t to avoid
    // extra type casting here.
    if (observer())
      observer()->OnOptionRemoved(index);
  }
}
