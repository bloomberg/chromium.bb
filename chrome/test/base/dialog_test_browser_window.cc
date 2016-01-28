// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/dialog_test_browser_window.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

using web_modal::WebContentsModalDialogHost;
using web_modal::ModalDialogHostObserver;

DialogTestBrowserWindow::DialogTestBrowserWindow() {
}

DialogTestBrowserWindow::~DialogTestBrowserWindow() {
}


WebContentsModalDialogHost*
DialogTestBrowserWindow::GetWebContentsModalDialogHost() {
  return this;
}

// The web contents modal dialog must be parented to *something*; use the
// WebContents window since there is no true browser window for unit tests.
gfx::NativeView DialogTestBrowserWindow::GetHostView() const {
  return FindBrowser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetNativeView();
}

gfx::Point DialogTestBrowserWindow::GetDialogPosition(const gfx::Size& size) {
  return gfx::Point();
}

gfx::Size DialogTestBrowserWindow::GetMaximumDialogSize() {
  return gfx::Size();
}

void DialogTestBrowserWindow::AddObserver(ModalDialogHostObserver* observer) {
}

void DialogTestBrowserWindow::RemoveObserver(
    ModalDialogHostObserver* observer) {
}

Browser* DialogTestBrowserWindow::FindBrowser() const {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->window() == this)
      return browser;
  }
  NOTREACHED();
  return nullptr;
}

