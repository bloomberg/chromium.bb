// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_DESKTOP_KEYBOARD_CAPTURE_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_DESKTOP_KEYBOARD_CAPTURE_H_

#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// This class installs/uninstalls keyboard hook as the widget activation
// changes.
class DesktopKeyboardCapture : public views::WidgetObserver {
 public:
  explicit DesktopKeyboardCapture(views::Widget* widget);
  ~DesktopKeyboardCapture() override;

 private:
  // views::WidgetObserver implementation.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  void RegisterKeyboardHooks();
  void DeregisterKeyboardHooks();

  // The widget registering for keyboard intercept. Weak reference.
  views::Widget* widget_;

  // Current state of keyboard intercept registration.
  bool is_registered_;

  DISALLOW_COPY_AND_ASSIGN(DesktopKeyboardCapture);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_DESKTOP_KEYBOARD_CAPTURE_H_
