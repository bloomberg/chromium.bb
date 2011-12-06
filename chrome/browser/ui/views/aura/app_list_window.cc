// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list_window.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/webui/aura/app_list_ui.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "ui/aura_shell/shell.h"
#include "ui/views/widget/widget.h"

AppListWindow::AppListWindow(const gfx::Rect& bounds,
    const aura_shell::ShellDelegate::SetWidgetCallback& callback)
    : widget_(NULL),
      contents_(NULL),
      callback_(callback),
      content_rendered_(false),
      apps_loaded_(false) {
  Init(bounds);
}

AppListWindow::~AppListWindow() {
}

void AppListWindow::DeleteDelegate() {
  delete this;
}

views::View* AppListWindow::GetInitiallyFocusedView() {
  return contents_;
}

views::Widget* AppListWindow::GetWidget() {
  return widget_;
}

const views::Widget* AppListWindow::GetWidget() const {
  return widget_;
}

bool AppListWindow::HandleContextMenu(const ContextMenuParams& params) {
  // Do not show the context menu for non-debug build.
#if !defined(NDEBUG)
  return false;
#else
  return true;
#endif
}

void AppListWindow::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (event.windowsKeyCode == ui::VKEY_ESCAPE)
    Close();
}

bool AppListWindow::TakeFocus(bool reverse) {
  // Forward the focus back to web contents.
  contents_->dom_contents()->tab_contents()->FocusThroughTabTraversal(reverse);
  return true;
}

bool AppListWindow::IsPopupOrPanel(const TabContents* source) const {
  return true;
}

void AppListWindow::OnRenderHostCreated(RenderViewHost* host) {
}

void AppListWindow::OnTabMainFrameLoaded() {
}

void AppListWindow::OnTabMainFrameFirstRender() {
  content_rendered_ = true;
  SetWidgetIfReady();
}

void AppListWindow::Close() {
  // We should be visible when running here and toggle actually closes us.
  aura_shell::Shell::GetInstance()->ToggleAppList();
}

void AppListWindow::OnAppsLoaded() {
  apps_loaded_ = true;
  SetWidgetIfReady();
}

void AppListWindow::Init(const gfx::Rect& bounds) {
  DCHECK(!widget_ && !contents_);

  contents_ = new DOMView();
  contents_->Init(ProfileManager::GetDefaultProfile(), NULL);

  TabContents* tab = contents_->dom_contents()->tab_contents();
  tab_watcher_.reset(new TabFirstRenderWatcher(tab, this));
  tab->set_delegate(this);

  contents_->LoadURL(GURL(chrome::kChromeUIAppListURL));
  static_cast<AppListUI*>(tab->web_ui())->set_delegate(this);

  // Use a background with transparency to trigger transparent webkit.
  SkBitmap background;
  background.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  background.allocPixels();
  background.eraseARGB(0x00, 0x00, 0x00, 0x00);

  RenderViewHost* host = tab->render_view_host();
  host->view()->SetBackground(background);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.bounds = bounds;
  widget_params.delegate = this;
  widget_params.keep_on_top = true;
  widget_params.transparent = true;

  widget_ = new views::Widget;
  widget_->Init(widget_params);
  widget_->SetContentsView(contents_);
}

void AppListWindow::SetWidgetIfReady() {
  if (content_rendered_ && apps_loaded_)
    callback_.Run(widget_);
}
