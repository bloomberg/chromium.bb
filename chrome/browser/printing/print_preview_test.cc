// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_test.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"

using web_modal::WebContentsModalDialogHost;
using web_modal::ModalDialogHostObserver;

namespace {

// Use a custom test browser window to provide a parent view to the print
// preview dialog.
class PrintPreviewTestBrowserWindow
    : public TestBrowserWindow,
      public WebContentsModalDialogHost {
 public:
  PrintPreviewTestBrowserWindow() {}

  // BrowserWindow overrides
  virtual WebContentsModalDialogHost* GetWebContentsModalDialogHost() OVERRIDE {
    return this;
  }

  // WebContentsModalDialogHost overrides

  // The web contents modal dialog must be parented to *something*; use the
  // WebContents window since there is no true browser window for unit tests.
  virtual gfx::NativeView GetHostView() const OVERRIDE {
    return FindBrowser()->tab_strip_model()->GetActiveWebContents()->
        GetNativeView();
  }

  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE {
    return gfx::Point();
  }

  virtual gfx::Size GetMaximumDialogSize() OVERRIDE {
    return gfx::Size();
  }

  virtual void AddObserver(
      ModalDialogHostObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(
      ModalDialogHostObserver* observer) OVERRIDE {}

 private:
  Browser* FindBrowser() const {
    for (chrome::BrowserIterator it; !it.done(); it.Next()) {
      Browser* browser = *it;
      if (browser->window() == this)
        return browser;
    }
    NOTREACHED();
    return NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewTestBrowserWindow);
};
}  // namespace

PrintPreviewTest::PrintPreviewTest() {}
PrintPreviewTest::~PrintPreviewTest() {}

void PrintPreviewTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();

  // The PluginService will be destroyed at the end of the test (due to the
  // ShadowingAtExitManager in our base class).
  content::PluginService::GetInstance()->Init();
  content::PluginService::GetInstance()->DisablePluginsDiscoveryForTesting();
}

BrowserWindow* PrintPreviewTest::CreateBrowserWindow() {
  return new PrintPreviewTestBrowserWindow;
}
