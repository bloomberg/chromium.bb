// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_WINDOW_COUNT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_WINDOW_COUNT_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

class Browser;

// The implementation for a bubble that allows the user to close all open
// incognito windows.
// The IncognitoWindowCountView is self-owned and deletes itself when it is
// closed or the parent browser is being destroyed.
class IncognitoWindowCountView : public views::BubbleDialogDelegateView,
                                 public BrowserListObserver,
                                 public views::ButtonListener {
 public:
  static void ShowBubble(views::Button* anchor_button,
                         Browser* browser,
                         int incognito_window_count);

  static bool IsShowing();

  // BubbleDialogDelegateView:
  int GetDialogButtons() const override;
  void Init() override;
  bool ShouldSnapFrameWidth() const override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  IncognitoWindowCountView(views::Button* anchor_button,
                           Browser* browser,
                           int incognito_window_count);

  ~IncognitoWindowCountView() override;

  int incognito_window_count_;
  Browser* const browser_;

  static IncognitoWindowCountView* incognito_window_counter_bubble_;

  ScopedObserver<BrowserList, BrowserListObserver> browser_list_observer_;
  base::WeakPtrFactory<IncognitoWindowCountView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IncognitoWindowCountView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_WINDOW_COUNT_VIEW_H_
