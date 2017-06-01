// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/remote_shelf_item_delegate.h"

namespace ash {

RemoteShelfItemDelegate::RemoteShelfItemDelegate(
    const ShelfID& shelf_id,
    mojom::ShelfItemDelegatePtr delegate)
    : ShelfItemDelegate(shelf_id), delegate_(std::move(delegate)) {}

RemoteShelfItemDelegate::~RemoteShelfItemDelegate() {}

void RemoteShelfItemDelegate::ItemSelected(std::unique_ptr<ui::Event> event,
                                           int64_t display_id,
                                           ShelfLaunchSource source,
                                           ItemSelectedCallback callback) {
  delegate_->ItemSelected(std::move(event), display_id, source,
                          std::move(callback));
}

void RemoteShelfItemDelegate::ExecuteCommand(uint32_t command_id,
                                             int32_t event_flags) {
  delegate_->ExecuteCommand(command_id, event_flags);
}

void RemoteShelfItemDelegate::Close() {
  delegate_->Close();
}

}  // namespace ash
