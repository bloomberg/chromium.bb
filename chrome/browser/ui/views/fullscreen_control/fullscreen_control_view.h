// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class FullscreenControlView : public views::View,
                              public DropdownBarHostDelegate,
                              public views::ButtonListener {
 public:
  explicit FullscreenControlView(BrowserView* browser_view);
  ~FullscreenControlView() override;

  // DropdownBarHostDelegate:
  void SetFocusAndSelection(bool select_all) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::Button* exit_fullscreen_button() { return exit_fullscreen_button_; }

 private:
  BrowserView* const browser_view_;
  views::Button* const exit_fullscreen_button_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControlView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_VIEW_H_
