// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/old_panel.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/panels/panel_browser_window.h"
#include "third_party/skia/include/core/SkBitmap.h"

OldPanel::OldPanel(Browser* browser,
                   const gfx::Size& min_size, const gfx::Size& max_size)
    : Panel(browser->app_name(), min_size, max_size),
      browser_(browser) {
}

OldPanel::~OldPanel() {}

Browser* OldPanel::browser() const {
  return browser_;
}

BrowserWindow* OldPanel::browser_window() const {
  return panel_browser_window_.get();
}

CommandUpdater* OldPanel::command_updater() {
  return browser_->command_controller()->command_updater();
}

Profile* OldPanel::profile() const {
  return browser_->profile();
}

void OldPanel::Initialize(const gfx::Rect& bounds, Browser* browser) {
  Panel::Initialize(bounds, browser);
  panel_browser_window_.reset(
      new PanelBrowserWindow(browser, this, native_panel()));
}

content::WebContents* OldPanel::GetWebContents() const {
  return chrome::GetActiveWebContents(browser_);
}

bool OldPanel::ShouldCloseWindow() {
  return browser_->ShouldCloseWindow();
}

void OldPanel::OnWindowClosing() {
  browser_->OnWindowClosing();
}

void OldPanel::ExecuteCommandWithDisposition(
    int id, WindowOpenDisposition disposition) {
  chrome::ExecuteCommandWithDisposition(browser_, id, disposition);
}

SkBitmap OldPanel::GetCurrentPageIcon() const {
  return browser_->GetCurrentPageIcon();
}
