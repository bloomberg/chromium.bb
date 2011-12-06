// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_first_render_watcher.h"
#include "chrome/browser/ui/webui/aura/app_list_ui_delegate.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/views/widget/widget_delegate.h"

class DOMView;

namespace views {
class Widget;
}

class AppListWindow : public views::WidgetDelegate,
                      public TabContentsDelegate,
                      public TabFirstRenderWatcher::Delegate,
                      public AppListUIDelegate {
 public:
  AppListWindow(
      const gfx::Rect& bounds,
      const aura_shell::ShellDelegate::SetWidgetCallback& callback);

 private:
  virtual ~AppListWindow();

  // views::WidgetDelegate overrides:
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // TabContentsDelegate implementation:
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsPopupOrPanel(const TabContents* source) const OVERRIDE;
  virtual bool TakeFocus(bool reverse) OVERRIDE;

  // TabFirstRenderWatcher::Delegate implementation:
  virtual void OnRenderHostCreated(RenderViewHost* host) OVERRIDE;
  virtual void OnTabMainFrameLoaded() OVERRIDE;
  virtual void OnTabMainFrameFirstRender() OVERRIDE;

  // AppListUIDelegate implementation:
  virtual void Close() OVERRIDE;
  virtual void OnAppsLoaded() OVERRIDE;

  // Initializes the window.
  void Init(const gfx::Rect& bounds);

  // Check and fire set widget callback if we are ready.
  void SetWidgetIfReady();

  views::Widget* widget_;
  DOMView* contents_;

  // Monitors TabContents and set |content_rendered_| flag when it's rendered.
  scoped_ptr<TabFirstRenderWatcher> tab_watcher_;

  // Callback to set app list widget when it's ready.
  aura_shell::ShellDelegate::SetWidgetCallback callback_;

  // True if webui is rendered.
  bool content_rendered_;

  // True if apps info is loaded by webui.
  bool apps_loaded_;

  DISALLOW_COPY_AND_ASSIGN(AppListWindow);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
