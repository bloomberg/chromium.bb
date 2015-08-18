// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "chrome/common/render_messages.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/error_page/common/net_error_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using chrome_browser_net::NetErrorTabHelper;
using error_page::DnsProbeStatus;

class TestNetErrorTabHelper : public NetErrorTabHelper {
 public:
  explicit TestNetErrorTabHelper(content::WebContents* web_contents)
      : NetErrorTabHelper(web_contents),
        mock_probe_running_(false),
        last_status_sent_(error_page::DNS_PROBE_MAX),
        mock_sent_count_(0),
        times_diagnostics_dialog_invoked_(0) {}

  void FinishProbe(DnsProbeStatus status) {
    EXPECT_TRUE(mock_probe_running_);
    OnDnsProbeFinished(status);
    mock_probe_running_ = false;
  }

  bool mock_probe_running() const { return mock_probe_running_; }
  DnsProbeStatus last_status_sent() const { return last_status_sent_; }
  int mock_sent_count() const { return mock_sent_count_; }

  const std::string& network_diagnostics_url() const {
    return network_diagnostics_url_;
  }

  int times_diagnostics_dialog_invoked() const {
    return times_diagnostics_dialog_invoked_;
  }

 private:
  // NetErrorTabHelper implementation:

  void StartDnsProbe() override {
    EXPECT_FALSE(mock_probe_running_);
    mock_probe_running_ = true;
  }

  void SendInfo() override {
    last_status_sent_ = dns_probe_status();
    mock_sent_count_++;
  }

  void RunNetworkDiagnosticsHelper(const std::string& sanitized_url) override {
    network_diagnostics_url_ = sanitized_url;
    times_diagnostics_dialog_invoked_++;
  }

  bool mock_probe_running_;
  DnsProbeStatus last_status_sent_;
  int mock_sent_count_;
  std::string network_diagnostics_url_;
  int times_diagnostics_dialog_invoked_;
};

class NetErrorTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  enum MainFrame { SUB_FRAME, MAIN_FRAME };
  enum ErrorPage { NORMAL_PAGE, ERROR_PAGE };
  enum ErrorType { DNS_ERROR, OTHER_ERROR };

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // This will simulate the initialization of the RenderFrame in the renderer
    // process. This is needed because WebContents does not initialize a
    // RenderFrame on construction, and the tests expect one to exist.
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();

    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");

    tab_helper_.reset(new TestNetErrorTabHelper(web_contents()));
    NetErrorTabHelper::set_state_for_testing(
        NetErrorTabHelper::TESTING_FORCE_ENABLED);
  }

  void TearDown() override {
    // Have to shut down the helper before the profile.
    tab_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void StartProvisionalLoad(MainFrame main_frame, ErrorPage error_page) {
    tab_helper_->DidStartProvisionalLoadForFrame(
        (main_frame == MAIN_FRAME) ? main_rfh() : subframe_,
        bogus_url_,  // validated_url
        (error_page == ERROR_PAGE),
        false);  // is_iframe_srcdoc
  }

  void CommitProvisionalLoad(MainFrame main_frame) {
    tab_helper_->DidCommitProvisionalLoadForFrame(
        (main_frame == MAIN_FRAME) ? main_rfh() : subframe_,
        bogus_url_,  // url
        ui::PAGE_TRANSITION_TYPED);
  }

  void FailProvisionalLoad(MainFrame main_frame, ErrorType error_type) {
    int net_error;

    if (error_type == DNS_ERROR)
      net_error = net::ERR_NAME_NOT_RESOLVED;
    else
      net_error = net::ERR_TIMED_OUT;

    tab_helper_->DidFailProvisionalLoad(
        (main_frame == MAIN_FRAME) ? main_rfh() : subframe_,
        bogus_url_,  // validated_url
        net_error,
        base::string16(),
        false);
  }

  void FinishProbe(DnsProbeStatus status) { tab_helper_->FinishProbe(status); }

  bool probe_running() { return tab_helper_->mock_probe_running(); }
  DnsProbeStatus last_status_sent() { return tab_helper_->last_status_sent(); }
  int sent_count() { return tab_helper_->mock_sent_count(); }

  TestNetErrorTabHelper* tab_helper() { return tab_helper_.get(); }

 private:
  content::RenderFrameHost* subframe_;
  scoped_ptr<TestNetErrorTabHelper> tab_helper_;
  GURL bogus_url_;
};

TEST_F(NetErrorTabHelperTest, Null) {
  EXPECT_FALSE(probe_running());
}

TEST_F(NetErrorTabHelperTest, MainFrameNonDnsError) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, OTHER_ERROR);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(0, sent_count());
}

TEST_F(NetErrorTabHelperTest, NonMainFrameDnsError) {
  StartProvisionalLoad(SUB_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(SUB_FRAME, DNS_ERROR);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(0, sent_count());
}

// Test complete DNS error page loads.  Note that the helper can see two error
// page loads: Link Doctor loads an empty HTML page so the user knows something
// is going on, then fails over to the normal error page if and when Link
// Doctor fails to load or declines to provide a page.

TEST_F(NetErrorTabHelperTest, ProbeResponseBeforeFirstCommit) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(0, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(1, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

TEST_F(NetErrorTabHelperTest, ProbeResponseBetweenFirstAndSecondCommit) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(2, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

TEST_F(NetErrorTabHelperTest, ProbeResponseAfterSecondCommit) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

// Send result even if a new page load has started; the error page is still
// visible, and the user might cancel the load.
TEST_F(NetErrorTabHelperTest, ProbeResponseAfterNewStart) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

// Don't send result if a new page has committed; the result would go to the
// wrong page, and the error page is gone anyway.
TEST_F(NetErrorTabHelperTest, ProbeResponseAfterNewCommit) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());

  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(2, sent_count());
}

TEST_F(NetErrorTabHelperTest, MultipleDnsErrorsWithProbesWithoutErrorPages) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(0, sent_count());

  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(0, sent_count());
}

TEST_F(NetErrorTabHelperTest, MultipleDnsErrorsWithProbesAndErrorPages) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(0, sent_count());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());

  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(4, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NO_INTERNET,
            last_status_sent());
}

// If multiple DNS errors occur in a row before a probe result, don't start
// multiple probes.
TEST_F(NetErrorTabHelperTest, CoalesceFailures) {
  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  StartProvisionalLoad(MAIN_FRAME, NORMAL_PAGE);
  FailProvisionalLoad(MAIN_FRAME, DNS_ERROR);
  StartProvisionalLoad(MAIN_FRAME, ERROR_PAGE);
  CommitProvisionalLoad(MAIN_FRAME);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(4, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

// Makes sure that URLs are sanitized before running the platform network
// diagnostics tool.
TEST_F(NetErrorTabHelperTest, SanitizeDiagnosticsUrl) {
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
  rfh->OnMessageReceived(ChromeViewHostMsg_RunNetworkDiagnostics(
      rfh->GetRoutingID(),
      GURL("http://foo:bar@somewhere:123/hats?for#goats")));
  EXPECT_EQ("http://somewhere:123/",
            tab_helper()->network_diagnostics_url());
  EXPECT_EQ(1, tab_helper()->times_diagnostics_dialog_invoked());
}

// Makes sure that diagnostics aren't run on invalid URLs or URLs with
// non-HTTP/HTTPS schemes.
TEST_F(NetErrorTabHelperTest, NoDiagnosticsForNonHttpSchemes) {
  const char* kUrls[] = {
    "",
    "http",
    "file:///blah/blah",
    "chrome://blah/",
    "about:blank",
    "file://foo/bar",
  };

  for (const char* url : kUrls) {
    content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
    rfh->OnMessageReceived(ChromeViewHostMsg_RunNetworkDiagnostics(
        rfh->GetRoutingID(), GURL(url)));
    EXPECT_EQ(0, tab_helper()->times_diagnostics_dialog_invoked());
  }
}
