// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_AUTO_SIGN_IN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_AUTO_SIGN_IN_VIEW_H_

#include "base/scoped_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class ManagePasswordsBubbleView;

// A view containing just one credential that was used for for automatic signing
// in.
class ManagePasswordAutoSignInView : public views::View,
                                     public views::ButtonListener,
                                     public views::WidgetObserver {
 public:
  explicit ManagePasswordAutoSignInView(ManagePasswordsBubbleView* parent);

#if defined(UNIT_TEST)
  static void set_auto_signin_toast_timeout(int seconds) {
    auto_signin_toast_timeout_ = seconds;
  }
#endif

 private:
  ~ManagePasswordAutoSignInView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetObserver:
  // Tracks the state of the browser window.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetClosing(views::Widget* widget) override;

  void OnTimer();
  static base::TimeDelta GetTimeout();

  base::OneShotTimer timer_;
  ManagePasswordsBubbleView* parent_;
  ScopedObserver<views::Widget, views::WidgetObserver> observed_browser_;

  // The timeout in seconds for the auto sign-in toast.
  static int auto_signin_toast_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordAutoSignInView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_AUTO_SIGN_IN_VIEW_H_
