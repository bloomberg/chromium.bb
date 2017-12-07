// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

class AutofillMetricsMetricsBrowserTest : public InProcessBrowserTest {
 public:
  AutofillMetricsMetricsBrowserTest() {}

  ~AutofillMetricsMetricsBrowserTest() override {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server_->InitializeAndListen());
    content::SetupCrossSiteRedirector(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    https_server_->StartAcceptingConnections();
  }

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillMetricsMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AutofillMetricsMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedAddressForm) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      https_server_->GetURL("b.com", "/autofill_address_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(AutofillMetricsMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedCreditCardForm) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      https_server_->GetURL("b.com", "/autofill_credit_card_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(AutofillMetricsMetricsBrowserTest,
                       CorrectSourceForUnownedAddressCheckout) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_unowned_address_checkout.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  // Make sure the UKM were logged for the main frame url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_EQ(main_frame_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(AutofillMetricsMetricsBrowserTest,
                       CorrectSourceForUnownedCreditCardCheckout) {
  GURL main_frame_url = https_server_->GetURL(
      "a.com", "/autofill_unowned_credit_card_checkout.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  // Make sure the UKM were logged for the main frame url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_EQ(main_frame_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(
    AutofillMetricsMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedAddressCheckout) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      https_server_->GetURL("b.com", "/autofill_unowned_address_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(
    AutofillMetricsMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedCreditCardCheckout) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = https_server_->GetURL(
      "b.com", "/autofill_unowned_credit_card_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}

class SitePerProcessAutofillMetricsMetricsBrowserTest
    : public AutofillMetricsMetricsBrowserTest {
 public:
  SitePerProcessAutofillMetricsMetricsBrowserTest() {}

  ~SitePerProcessAutofillMetricsMetricsBrowserTest() override {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillMetricsMetricsBrowserTest::SetUpCommandLine(command_line);

    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessAutofillMetricsMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedAddressForm) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      https_server_->GetURL("b.com", "/autofill_address_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(), tab->GetMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedCreditCardForm) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      https_server_->GetURL("b.com", "/autofill_credit_card_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(), tab->GetMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsMetricsBrowserTest,
                       CorrectSourceForUnownedAddressCheckout) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_unowned_address_checkout.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  // Make sure the UKM were logged for the main frame url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_EQ(main_frame_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsMetricsBrowserTest,
                       CorrectSourceForUnownedCreditCardCheckout) {
  GURL main_frame_url = https_server_->GetURL(
      "a.com", "/autofill_unowned_credit_card_checkout.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  // Make sure the UKM were logged for the main frame url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_EQ(main_frame_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(
    SitePerProcessAutofillMetricsMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedAddressCheckout) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      https_server_->GetURL("b.com", "/autofill_unowned_address_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(), tab->GetMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}

IN_PROC_BROWSER_TEST_F(
    SitePerProcessAutofillMetricsMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedCreditCardCheckout) {
  GURL main_frame_url =
      https_server_->GetURL("a.com", "/autofill_iframe_embedder.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = https_server_->GetURL(
      "b.com", "/autofill_unowned_credit_card_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(), tab->GetMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url.host(), kv.second->url().host());
  }
}
