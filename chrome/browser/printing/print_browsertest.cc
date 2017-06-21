// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace printing {

namespace {

class PrintPreviewObserver : PrintPreviewUI::TestingDelegate {
 public:
  PrintPreviewObserver() { PrintPreviewUI::SetDelegateForTesting(this); }
  ~PrintPreviewObserver() { PrintPreviewUI::SetDelegateForTesting(nullptr); }

  void WaitUntilPreviewIsReady() {
    if (rendered_page_count_ >= total_page_count_)
      return;

    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

 private:
  // PrintPreviewUI::TestingDelegate implementation.
  void DidGetPreviewPageCount(int page_count) override {
    total_page_count_ = page_count;
  }

  // PrintPreviewUI::TestingDelegate implementation.
  void DidRenderPreviewPage(content::WebContents* preview_dialog) override {
    ++rendered_page_count_;
    CHECK(rendered_page_count_ <= total_page_count_);
    if (rendered_page_count_ == total_page_count_ && run_loop_) {
      run_loop_->Quit();
    }
  }

  int total_page_count_ = 1;
  int rendered_page_count_ = 0;
  base::RunLoop* run_loop_ = nullptr;
};

}  // namespace

class PrintBrowserTest : public InProcessBrowserTest {
 public:
  PrintBrowserTest() {}
  ~PrintBrowserTest() override {}

  void PrintAndWaitUntilPreviewIsReady(bool print_only_selection) {
    PrintPreviewObserver print_preview_observer;

    printing::StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
                         /*print_preview_disabled=*/false,
                         print_only_selection);

    print_preview_observer.WaitUntilPreviewIsReady();
  }
};

// Printing only a selection containing iframes is partially supported.
// Iframes aren't currently displayed. This test passes whenever the print
// preview is rendered (i.e. no timeout in the test).
// This test shouldn't crash. See https://crbug.com/732780.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, SelectionContainsIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/printing/selection_iframe.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/true);
}

}  // namespace printing
