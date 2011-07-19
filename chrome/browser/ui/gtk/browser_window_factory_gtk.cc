// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/find_bar_gtk.h"

BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  BrowserWindowGtk* browser_window_gtk = new BrowserWindowGtk(browser);
  browser_window_gtk->Init();
  return browser_window_gtk;
}

FindBar* BrowserWindow::CreateFindBar(Browser* browser) {
  return new FindBarGtk(browser);
}
