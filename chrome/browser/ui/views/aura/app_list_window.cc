// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list_window.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "views/widget/widget.h"

AppListWindow::AppListWindow(
    const aura_shell::ShellDelegate::SetWidgetCallback& callback)
    : widget_(NULL),
      contents_(NULL),
      callback_(callback) {
  Init();
}

AppListWindow::~AppListWindow() {
}

void AppListWindow::DeleteDelegate() {
  delete this;
}

views::View* AppListWindow::GetContentsView() {
  return contents_;
}

views::Widget* AppListWindow::GetWidget() {
  return widget_;
}

const views::Widget* AppListWindow::GetWidget() const {
  return widget_;
}

void AppListWindow::OnRenderHostCreated(RenderViewHost* host) {
}

void AppListWindow::OnTabMainFrameLoaded() {
}

void AppListWindow::OnTabMainFrameFirstRender() {
  callback_.Run(widget_);
}

void AppListWindow::Init() {
  DCHECK(!widget_ && !contents_);

  contents_ = new DOMView();
  contents_->Init(ProfileManager::GetDefaultProfile(), NULL);

  TabContents* tab = contents_->dom_contents()->tab_contents();
  tab_watcher_.reset(new TabFirstRenderWatcher(tab, this));

  contents_->LoadURL(GURL(chrome::kChromeUIAppListURL));

  // Use a background with transparency to trigger transparent webkit.
  SkBitmap background;
  background.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  background.allocPixels();
  background.eraseARGB(0x00, 0x00, 0x00, 0x00);

  RenderViewHost* host = tab->render_view_host();
  host->view()->SetBackground(background);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // A non-empty bounds so that we get rendered notification. Make the size
  // close the final size so that card slider resize handler does no generate
  // unexpected animation.
  widget_params.bounds = gfx::Rect(0, 0, 900, 700);
  widget_params.delegate = this;
  widget_params.keep_on_top = true;
  widget_params.transparent = true;

  widget_ = new views::Widget;
  widget_->Init(widget_params);
  widget_->SetContentsView(contents_);
}
