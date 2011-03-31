// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/dom_browser.h"

#include "chrome/browser/chromeos/frame/dom_browser_view.h"
#include "chrome/browser/ui/browser_window.h"

namespace chromeos {

// DOMBrowser: public ----------------------------------------------------------

DOMBrowser::DOMBrowser(Profile* profile)
    : Browser(Browser::TYPE_NORMAL, profile) {
}

DOMBrowser::~DOMBrowser() {
}

// static
DOMBrowser* DOMBrowser::CreateForDOM(Profile* profile) {
  DOMBrowser* browser = new DOMBrowser(profile);
  browser->InitBrowserWindow();
  return browser;
}

BrowserWindow* DOMBrowser::CreateBrowserWindow() {
  return DOMBrowserView::CreateDOMWindow(this);
}

}  // namespace chromeos
