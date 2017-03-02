// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_TEST_SHELF_ITEM_DELEGATE_H_
#define ASH_COMMON_TEST_TEST_SHELF_ITEM_DELEGATE_H_

#include "ash/common/shelf/shelf_item_delegate.h"
#include "base/macros.h"

namespace ash {

class WmWindow;

namespace test {

// Test implementation of ShelfItemDelegate.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(WmWindow* window);
  ~TestShelfItemDelegate() override;

  // ShelfItemDelegate:
  ShelfAction ItemSelected(ui::EventType event_type,
                           int event_flags,
                           int64_t display_id,
                           ShelfLaunchSource source) override;
  ShelfAppMenuItemList GetAppMenuItems(int event_flags) override;
  void ExecuteCommand(uint32_t command_id, int event_flags) override;
  void Close() override;

 private:
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfItemDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_COMMON_TEST_TEST_SHELF_ITEM_DELEGATE_H_
