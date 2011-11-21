// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_first_render_watcher.h"
#include "ui/aura_shell/shell_delegate.h"
#include "views/widget/widget_delegate.h"

class DOMView;

namespace views {
class Widget;
}

class AppListWindow : public views::WidgetDelegate,
                      public TabFirstRenderWatcher::Delegate {
 public:
  explicit AppListWindow(
      const aura_shell::ShellDelegate::SetWidgetCallback& callback);

 private:
  virtual ~AppListWindow();

  // views::WidgetDelegate overrides:
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // TabFirstRenderWatcher::Delegate implementation:
  virtual void OnRenderHostCreated(RenderViewHost* host) OVERRIDE;
  virtual void OnTabMainFrameLoaded() OVERRIDE;
  virtual void OnTabMainFrameFirstRender() OVERRIDE;

  // Initializes the window.
  void Init();

  views::Widget* widget_;
  DOMView* contents_;

  // Monitors TabContents and set |content_rendered_| flag when it's rendered.
  scoped_ptr<TabFirstRenderWatcher> tab_watcher_;

  // Callback to set app list widget when it's ready.
  aura_shell::ShellDelegate::SetWidgetCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AppListWindow);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
