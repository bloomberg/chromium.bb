// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#pragma once

#include "ui/aura/desktop_observer.h"
#include "views/widget/widget_delegate.h"

class DOMView;

namespace views {
class Widget;
}

class AppListWindow : public views::WidgetDelegate,
                      public aura::DesktopObserver {
 public:
  // Show/hide app list window.
  static void SetVisible(bool visible);

  // Whether app list window is visible (shown or being shown).
  static bool IsVisible();

 private:
  AppListWindow();
  virtual ~AppListWindow();

  // Overridden from views::WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // aura::DesktopObserver overrides:
  virtual void OnActiveWindowChanged(aura::Window* active) OVERRIDE;

  // Initializes the window.
  void Init();

  // Shows/hides the window.
  void SetVisible(bool visible, bool animate);

  bool is_visible() const {
    return is_visible_;
  }

  // Current visible app list window.
  static AppListWindow* instance_;

  views::Widget* widget_;
  DOMView* contents_;
  bool is_visible_;

  DISALLOW_COPY_AND_ASSIGN(AppListWindow);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
