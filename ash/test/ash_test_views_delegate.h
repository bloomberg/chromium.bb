// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_
#define ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_

#include "base/macros.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {

class TestAccessibilityEventDelegate {
 public:
  TestAccessibilityEventDelegate() {}
  virtual ~TestAccessibilityEventDelegate() {}
  virtual void NotifyAccessibilityEvent(views::View* view,
                                        ax::mojom::Event event_type) = 0;
};

// Ash specific ViewsDelegate. In addition to creating a TestWebContents this
// parents widget with no parent/context to the shell. This is created by
// default AshTestHelper.
class AshTestViewsDelegate : public views::TestViewsDelegate {
 public:
  AshTestViewsDelegate();
  ~AshTestViewsDelegate() override;

  // Call this only if this code is being run outside of ash, for example, in
  // browser tests that use AshTestBase. This disables CHECKs that are
  // applicable only when used inside ash.
  // TODO: remove this and ban usage of AshTestHelper outside of ash.
  void set_running_outside_ash() { running_outside_ash_ = true; }

  // Not owned.
  void set_test_accessibility_event_delegate(
      TestAccessibilityEventDelegate* test_accessibility_event_delegate) {
    test_accessibility_event_delegate_ = test_accessibility_event_delegate;
  }

  // Overriden from TestViewsDelegate.
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override;
  void NotifyAccessibilityEvent(views::View* view,
                                ax::mojom::Event event_type) override;
  views::TestViewsDelegate::ProcessMenuAcceleratorResult
  ProcessAcceleratorWhileMenuShowing(
      const ui::Accelerator& accelerator) override;

  void set_close_menu_accelerator(const ui::Accelerator& accelerator) {
    close_menu_accelerator_ = accelerator;
  }

 private:
  // ProcessAcceleratorWhileMenuShowing returns CLOSE_MENU if passed accelerator
  // matches with this.
  ui::Accelerator close_menu_accelerator_;

  // Not owned.
  TestAccessibilityEventDelegate* test_accessibility_event_delegate_ = nullptr;

  bool running_outside_ash_ = false;

  DISALLOW_COPY_AND_ASSIGN(AshTestViewsDelegate);
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_
