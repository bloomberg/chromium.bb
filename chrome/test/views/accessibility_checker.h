// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VIEWS_ACCESSIBILITY_CHECKER_H_
#define CHROME_TEST_VIEWS_ACCESSIBILITY_CHECKER_H_

#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "ui/views/widget/widget_observer.h"

// Runs UI accessibility checks on |widget|.
// Adds a gtest failure if any check fails.
// Callers are not expected to assert/expect on failure.
void AddFailureOnWidgetAccessibilityError(views::Widget* widget);

// Observe the creation of all widgets and ensure their view subtrees are
// checked for accessibility violations when they become visible or hidden.
//
// Accessibility violations will add a gtest failure.
class AccessibilityChecker : public ChromeViewsDelegate,
                             public views::WidgetObserver {
 public:
  AccessibilityChecker();
  ~AccessibilityChecker() override;

  // ChromeViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

 private:
  ScopedObserver<views::Widget, WidgetObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityChecker);
};

#endif  // CHROME_TEST_VIEWS_ACCESSIBILITY_CHECKER_H_
