// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_
#define ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_

#include "base/macros.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {
namespace test {

class TestAccessibilityEventDelegate {
 public:
  TestAccessibilityEventDelegate() {}
  virtual ~TestAccessibilityEventDelegate() {}
  virtual void NotifyAccessibilityEvent(views::View* view,
                                        ui::AXEvent event_type) = 0;
};

// Ash specific ViewsDelegate. In addition to creating a TestWebContents this
// parents widget with no parent/context to the shell. This is created by
// default AshTestHelper.
class AshTestViewsDelegate : public views::TestViewsDelegate {
 public:
  AshTestViewsDelegate();
  ~AshTestViewsDelegate() override;

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
                                ui::AXEvent event_type) override;

 private:
  // Not owned.
  TestAccessibilityEventDelegate* test_accessibility_event_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AshTestViewsDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_
