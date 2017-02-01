// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/renderer_context_menu/spelling_bubble_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"

class AskGoogleForSuggestionsDialogTest : public DialogBrowserTest {
 public:
  AskGoogleForSuggestionsDialogTest() {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    std::unique_ptr<SpellingBubbleModel> model =
        base::MakeUnique<SpellingBubbleModel>(
            browser()->profile(),
            browser()->tab_strip_model()->GetActiveWebContents());

    // The toolkit-views version of the dialog does not utilize the anchor_view
    // and origin parameters passed to this function. Pass dummy values.
    chrome::ShowConfirmBubble(browser()->window()->GetNativeWindow(), nullptr,
                              gfx::Point(), std::move(model));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AskGoogleForSuggestionsDialogTest);
};

#if !defined(OS_MACOSX)
// Initially disabled except on Mac due to http://crbug.com/683808.
#define MAYBE_InvokeDialog_default DISABLED_InvokeDialog_default
#else
#define MAYBE_InvokeDialog_default InvokeDialog_default
#endif

// Test that calls ShowDialog("default"). Interactive when run via
// browser_tests --gtest_filter=BrowserDialogTest.Invoke --interactive
// --dialog=AskGoogleForSuggestionsDialogTest.InvokeDialog_default
IN_PROC_BROWSER_TEST_F(AskGoogleForSuggestionsDialogTest,
                       MAYBE_InvokeDialog_default) {
  RunDialog();
}
