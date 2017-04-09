// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_
#define ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_

#include "ash/public/cpp/shelf_item_delegate.h"
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
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    const ItemSelectedCallback& callback) override;
  void ExecuteCommand(uint32_t command_id, int32_t event_flags) override;
  void Close() override;

 private:
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfItemDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_
