// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_
#define ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_

#include "ash/shelf/shelf_item_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace ash {
namespace test {

// Test implementation of ShelfItemDelegate.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(aura::Window* window);
  ~TestShelfItemDelegate() override;

  // ShelfItemDelegate:
  bool ItemSelected(const ui::Event& event) override;
  base::string16 GetTitle() override;
  ui::MenuModel* CreateContextMenu(aura::Window* root_window) override;
  ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  bool IsDraggable() override;
  bool ShouldShowTooltip() override;
  void Close() override;

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfItemDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_
