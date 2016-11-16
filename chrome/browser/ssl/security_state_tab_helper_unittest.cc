// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_tab_helper.h"

#include "base/command_line.h"
#include "base/test/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/security_state/core/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SecurityStateTabHelperHistogramTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  SecurityStateTabHelperHistogramTest() : helper_(nullptr) {}
  ~SecurityStateTabHelperHistogramTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SecurityStateTabHelper::CreateForWebContents(web_contents());
    helper_ = SecurityStateTabHelper::FromWebContents(web_contents());
    navigate_to_http();
  }

 protected:
  void signal_sensitive_input() {
    if (GetParam())
      web_contents()->OnPasswordInputShownOnHttp();
    else
      web_contents()->OnCreditCardInputShownOnHttp();
    helper_->VisibleSecurityStateChanged();
  }

  const std::string histogram_name() {
    if (GetParam())
      return "Security.HTTPBad.UserWarnedAboutSensitiveInput.Password";
    else
      return "Security.HTTPBad.UserWarnedAboutSensitiveInput.CreditCard";
  }

  void navigate_to_http() { NavigateAndCommit(GURL("http://example.test")); }

  void navigate_to_different_http_page() {
    NavigateAndCommit(GURL("http://example2.test"));
  }

 private:
  SecurityStateTabHelper* helper_;
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperHistogramTest);
};

// Tests that UMA logs the omnibox warning when security level is
// HTTP_SHOW_WARNING.
TEST_P(SecurityStateTabHelperHistogramTest, HTTPOmniboxWarningHistogram) {
  // Show Warning Chip.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpWithPasswordsOrCcWithChip);

  base::HistogramTester histograms;
  signal_sensitive_input();
  histograms.ExpectUniqueSample(histogram_name(), true, 1);

  // Fire again and ensure no sample is recorded.
  signal_sensitive_input();
  histograms.ExpectUniqueSample(histogram_name(), true, 1);

  // Navigate to a new page and ensure a sample is recorded.
  navigate_to_different_http_page();
  histograms.ExpectUniqueSample(histogram_name(), true, 1);
  signal_sensitive_input();
  histograms.ExpectUniqueSample(histogram_name(), true, 2);
}

// Tests that UMA logs the console warning when security level is NONE.
TEST_P(SecurityStateTabHelperHistogramTest, HTTPConsoleWarningHistogram) {
  // Show Neutral for HTTP
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpAsNeutral);

  base::HistogramTester histograms;
  signal_sensitive_input();
  histograms.ExpectUniqueSample(histogram_name(), false, 1);

  // Fire again and ensure no sample is recorded.
  signal_sensitive_input();
  histograms.ExpectUniqueSample(histogram_name(), false, 1);

  // Navigate to a new page and ensure a sample is recorded.
  navigate_to_different_http_page();
  histograms.ExpectUniqueSample(histogram_name(), false, 1);
  signal_sensitive_input();
  histograms.ExpectUniqueSample(histogram_name(), false, 2);
}

INSTANTIATE_TEST_CASE_P(SecurityStateTabHelperHistogramTest,
                        SecurityStateTabHelperHistogramTest,
                        // Here 'true' to test password field triggered
                        // histogram and 'false' to test credit card field.
                        testing::Bool());

}  // namespace
