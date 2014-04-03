// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_details_view.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

SystemTray* GetSystemTray() {
  return Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->system_tray();
}

class TestDetailsView : public TrayDetailsView, public ViewClickListener {
 public:
  explicit TestDetailsView(SystemTrayItem* owner) : TrayDetailsView(owner) {}

  virtual ~TestDetailsView() {}

  void CreateFooterAndFocus() {
    // Uses bluetooth label for testing purpose. It can be changed to any
    // string_id.
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_BLUETOOTH, this);
    footer()->content()->RequestFocus();
  }

  // Overridden from ViewClickListener:
  virtual void OnViewClicked(views::View* sender) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDetailsView);
};

// Trivial item implementation that tracks its views for testing.
class TestItem : public SystemTrayItem {
 public:
  TestItem() : SystemTrayItem(GetSystemTray()), tray_view_(NULL) {}

  // Overridden from SystemTrayItem:
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE {
    tray_view_ = new views::View;
    return tray_view_;
  }
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE {
    default_view_ = new views::View;
    default_view_->SetFocusable(true);
    return default_view_;
  }
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE {
    detailed_view_ = new TestDetailsView(this);
    return detailed_view_;
  }
  virtual void DestroyTrayView() OVERRIDE {
    tray_view_ = NULL;
  }
  virtual void DestroyDefaultView() OVERRIDE {
    default_view_ = NULL;
  }
  virtual void DestroyDetailedView() OVERRIDE {
    detailed_view_ = NULL;
  }

  views::View* tray_view() const { return tray_view_; }
  views::View* default_view() const { return default_view_; }
  TestDetailsView* detailed_view() const { return detailed_view_; }

 private:
  views::View* tray_view_;
  views::View* default_view_;
  TestDetailsView* detailed_view_;

  DISALLOW_COPY_AND_ASSIGN(TestItem);
};

}  // namespace

typedef AshTestBase TrayDetailsViewTest;

TEST_F(TrayDetailsViewTest, TransitionToDefaultViewTest) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestItem* test_item_1 = new TestItem;
  TestItem* test_item_2 = new TestItem;
  tray->AddTrayItem(test_item_1);
  tray->AddTrayItem(test_item_2);

  // Ensure the tray views are created.
  ASSERT_TRUE(test_item_1->tray_view() != NULL);
  ASSERT_TRUE(test_item_2->tray_view() != NULL);

  // Show the default view.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();

  // Show the detailed view of item 2.
  tray->ShowDetailedView(test_item_2, 0, true, BUBBLE_USE_EXISTING);
  EXPECT_TRUE(test_item_2->detailed_view());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(test_item_2->default_view());

  // Transition back to default view, the default view of item 2 should have
  // focus.
  test_item_2->detailed_view()->CreateFooterAndFocus();
  test_item_2->detailed_view()->TransitionToDefaultView();
  RunAllPendingInMessageLoop();

  EXPECT_TRUE(test_item_2->default_view());
  EXPECT_FALSE(test_item_2->detailed_view());
  EXPECT_TRUE(test_item_2->default_view()->HasFocus());

  // Show the detailed view of item 2 again.
  tray->ShowDetailedView(test_item_2, 0, true, BUBBLE_USE_EXISTING);
  EXPECT_TRUE(test_item_2->detailed_view());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(test_item_2->default_view());

  // Transition back to default view, the default view of item 2 should NOT have
  // focus.
  test_item_2->detailed_view()->TransitionToDefaultView();
  RunAllPendingInMessageLoop();

  EXPECT_TRUE(test_item_2->default_view());
  EXPECT_FALSE(test_item_2->detailed_view());
  EXPECT_FALSE(test_item_2->default_view()->HasFocus());
}

}  // namespace test
}  // namespace ash
