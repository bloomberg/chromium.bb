// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.cc"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/browser/web_contents_unresponsive_state.h"
#include "ui/base/ui_base_switches.h"

class HungRendererDialogViewBrowserTest : public DialogBrowserTest {
 public:
  HungRendererDialogViewBrowserTest() {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    TabDialogs::FromWebContents(web_contents)
        ->ShowHungRendererDialog(content::WebContentsUnresponsiveState());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogViewBrowserTest);
};

// TODO(tapted): On OSX the framework doesn't pick up the spawned dialog, and
// the ASSERT_EQ in TestBrowserDialog::RunDialog() fails, so disabled for now.
#if defined(OS_MACOSX)
#define MAYBE_InvokeDialog_default DISABLED_InvokeDialog_default
#else
#define MAYBE_InvokeDialog_default InvokeDialog_default
#endif
// Invokes the hung renderer (aka page unresponsive) dialog. See
// test_browser_dialog.h.
IN_PROC_BROWSER_TEST_F(HungRendererDialogViewBrowserTest,
                       MAYBE_InvokeDialog_default) {
  RunDialog();
}
