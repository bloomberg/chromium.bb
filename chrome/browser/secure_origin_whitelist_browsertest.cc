// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace {
// SecureOriginWhitelistBrowsertests differ in the setup of the browser. Since
// the setup is done before the actual test is run, we need to parameterize our
// tests outside of the actual test bodies. We use test variants for this,
// instead of the usual setup of mulitple tests.
enum class TestVariant { kNone, kCommandline };
}  // namespace

// End-to-end browser test that ensures the secure origin whitelist works when
// supplied via the command-line.
// SecureOriginWhitelistUnittest will test the list parsing.
class SecureOriginWhitelistBrowsertest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestVariant> {
 public:
  void SetUpOnMainThread() override {
    // We need this, so we can request the test page from 'http://foo.com'.
    // (Which, unlike 127.0.0.1, is considered an insecure origin.)
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // We need to know the server port to know what to add to the command-line.
    // The port number changes with every test run. Thus, we start the server
    // here. And since all tests, not just the variant with the command-line,
    // need the embedded server, we unconditionally start it here.
    EXPECT_TRUE(embedded_test_server()->Start());

    if (GetParam() != TestVariant::kCommandline)
      return;

    command_line->AppendSwitchASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure, BaseURL());
  }

  bool ExpectSecureContext() { return GetParam() != TestVariant::kNone; }

  std::string BaseURL() {
    return embedded_test_server()->GetURL("example.com", "/").spec();
  }
};

INSTANTIATE_TEST_CASE_P(SecureOriginWhitelistBrowsertest,
                        SecureOriginWhitelistBrowsertest,
                        testing::Values(TestVariant::kNone,
                                        TestVariant::kCommandline));

IN_PROC_BROWSER_TEST_P(SecureOriginWhitelistBrowsertest, Simple) {
  GURL url = embedded_test_server()->GetURL(
      "example.com", "/secure_origin_whitelist_browsertest.html");
  ui_test_utils::NavigateToURL(browser(), url);

  base::string16 secure(base::ASCIIToUTF16("secure context"));
  base::string16 insecure(base::ASCIIToUTF16("insecure context"));

  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), secure);
  title_watcher.AlsoWaitForTitle(insecure);

  EXPECT_EQ(title_watcher.WaitAndGetTitle(),
            ExpectSecureContext() ? secure : insecure);
}
