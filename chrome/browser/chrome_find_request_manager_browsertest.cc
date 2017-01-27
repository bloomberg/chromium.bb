// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/find_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

namespace content {

class ChromeFindRequestManagerTest : public InProcessBrowserTest {
 public:
  ChromeFindRequestManagerTest()
      : normal_delegate_(nullptr),
        last_request_id_(0) {}
  ~ChromeFindRequestManagerTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Swap the WebContents's delegate for our test delegate.
    normal_delegate_ = contents()->GetDelegate();
    contents()->SetDelegate(&test_delegate_);
  }

  void TearDownOnMainThread() override {
    // Swap the WebContents's delegate back to its usual delegate.
    contents()->SetDelegate(normal_delegate_);
  }

 protected:
  // Navigates to |url| and waits for it to finish loading.
  void LoadAndWait(const std::string& url) {
    TestNavigationObserver navigation_observer(contents());
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), embedded_test_server()->GetURL("a.com", url), 1);
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  void Find(const std::string& search_text,
            const blink::WebFindOptions& options) {
    delegate()->UpdateLastRequest(++last_request_id_);
    contents()->Find(last_request_id_,
                     base::UTF8ToUTF16(search_text),
                     options);
  }

  WebContents* contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FindTestWebContentsDelegate* delegate() const {
    return static_cast<FindTestWebContentsDelegate*>(contents()->GetDelegate());
  }

  int last_request_id() const {
    return last_request_id_;
  }

 private:
  FindTestWebContentsDelegate test_delegate_;
  WebContentsDelegate* normal_delegate_;

  // The ID of the last find request requested.
  int last_request_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFindRequestManagerTest);
};


// Tests searching in a full-page PDF.
IN_PROC_BROWSER_TEST_F(ChromeFindRequestManagerTest, FindInPDF) {
  LoadAndWait("/find_in_pdf_page.pdf");
  pdf_extension_test_util::EnsurePDFHasLoaded(contents());

  blink::WebFindOptions options;
  Find("result", options);
  options.findNext = true;
  Find("result", options);
  Find("result", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(3, results.active_match_ordinal);
}

}  // namespace content
