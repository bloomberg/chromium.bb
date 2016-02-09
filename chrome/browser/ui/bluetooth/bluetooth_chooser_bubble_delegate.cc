// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_bubble_delegate.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "components/bubble/bubble_controller.h"

BluetoothChooserBubbleDelegate::BluetoothChooserBubbleDelegate(
    content::RenderFrameHost* owner)
    : ChooserBubbleDelegate(owner), bluetooth_chooser_(nullptr) {}

BluetoothChooserBubbleDelegate::~BluetoothChooserBubbleDelegate() {
  if (bluetooth_chooser_)
    bluetooth_chooser_->set_bluetooth_chooser_bubble_delegate(nullptr);
}

size_t BluetoothChooserBubbleDelegate::NumOptions() const {
  return device_names_and_ids_.size();
}

const base::string16& BluetoothChooserBubbleDelegate::GetOption(
    size_t index) const {
  DCHECK_LT(index, device_names_and_ids_.size());
  return device_names_and_ids_[index].first;
}

void BluetoothChooserBubbleDelegate::Select(size_t index) {
  DCHECK_LT(index, device_names_and_ids_.size());
  if (bluetooth_chooser_) {
    bluetooth_chooser_->CallEventHandler(
        content::BluetoothChooser::Event::SELECTED,
        device_names_and_ids_[index].second);
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
  device_names_and_ids_.push_back(std::make_pair(device_name, device_id));
  if (observer())
    observer()->OnOptionAdded(device_names_and_ids_.size() - 1);
}

void BluetoothChooserBubbleDelegate::RemoveDevice(
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
