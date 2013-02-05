// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/common/net/net_error_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_common_net::DnsProbeResult;
using content::BrowserThread;
using content::TestBrowserThread;

namespace chrome_browser_net {

namespace {

class TestNetErrorTabHelper : public NetErrorTabHelper {
 public:
  TestNetErrorTabHelper()
      : NetErrorTabHelper(NULL),
        probe_count_(0),
        send_count_(0) {
  }

  void SimulateProbeResult(DnsProbeResult result) {
    OnDnsProbeFinishedForTesting(result);
  }

  int probe_count() { return probe_count_; }
  int send_count() { return send_count_; }
  DnsProbeResult send_result() { return send_result_; }

 private:
  // NetErrorTabHelper implementation:

  virtual void PostStartDnsProbeTask() OVERRIDE { ++probe_count_; }
  virtual void SendInfo() OVERRIDE {
    ++send_count_;
    send_result_ = dns_probe_result();
  }

 private:
  int probe_count_;
  int send_count_;
  DnsProbeResult send_result_;
};

class NetErrorTabHelperTest : public testing::Test {
 public:
  NetErrorTabHelperTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
    NetErrorTabHelper::set_state_for_testing(
        NetErrorTabHelper::TESTING_FORCE_ENABLED);
  }

 protected:
  enum MainFrame { NOT_MAIN_FRAME, MAIN_FRAME };
  enum ErrorPage { NORMAL_PAGE, ERROR_PAGE };

  void SimulateStart(MainFrame main_frame, ErrorPage error_page) {
    GURL validated_url;

    tab_helper_.DidStartProvisionalLoadForFrame(
        1 /* frame_id */,
        0 /* parent_frame_id */,
        (main_frame == MAIN_FRAME),
        validated_url,
        (error_page == ERROR_PAGE),
        false /* is_iframe_srcdoc */,
        NULL /* render_view_host */);
  }

  void SimulateCommit(MainFrame main_frame) {
    GURL url;

    tab_helper_.DidCommitProvisionalLoadForFrame(
        1 /* frame_id */,
        (main_frame == MAIN_FRAME),
        url,
        content::PAGE_TRANSITION_TYPED,
        NULL /* render_view_host */);
  }

  void SimulateFail(MainFrame main_frame, int error_code) {
    GURL url;
    string16 error_description;

    tab_helper_.DidFailProvisionalLoad(
        1 /* frame_id */,
        (main_frame == MAIN_FRAME),
        url,
        error_code,
        error_description,
        NULL /* render_view_host */);
  }

  void SimulateFinish(MainFrame main_frame) {
    GURL validated_url;

    tab_helper_.DidFinishLoad(
        1 /* frame_id */,
        validated_url,
        (main_frame == MAIN_FRAME),
        NULL /* render_view_host */);
  }

  void SimulateMainFrameDnsError() {
    SimulateStart(MAIN_FRAME, NORMAL_PAGE);
    SimulateFail(MAIN_FRAME, net::ERR_NAME_NOT_RESOLVED);
  }

  void SimulateMainFrameErrorPageLoad() {
    SimulateStart(MAIN_FRAME, ERROR_PAGE);
    SimulateCommit(MAIN_FRAME);
    SimulateFinish(MAIN_FRAME);
  }

  MessageLoop message_loop_;
  TestBrowserThread ui_thread_;

  TestNetErrorTabHelper tab_helper_;
};

const DnsProbeResult kSampleResult = chrome_common_net::DNS_PROBE_NXDOMAIN;
const DnsProbeResult kUnknownResult = chrome_common_net::DNS_PROBE_UNKNOWN;

TEST_F(NetErrorTabHelperTest, InitialState) {
  EXPECT_EQ(0, tab_helper_.probe_count());
}

TEST_F(NetErrorTabHelperTest, NoProbeOnNonDnsError) {
  SimulateStart(MAIN_FRAME, NORMAL_PAGE);
  SimulateFail(MAIN_FRAME, net::ERR_FAILED);
  EXPECT_EQ(0, tab_helper_.probe_count());
}

TEST_F(NetErrorTabHelperTest, NoSendOnNonDnsError) {
  SimulateStart(MAIN_FRAME, NORMAL_PAGE);
  SimulateFail(MAIN_FRAME, net::ERR_FAILED);
  SimulateStart(MAIN_FRAME, ERROR_PAGE);
  SimulateCommit(MAIN_FRAME);
  SimulateFinish(MAIN_FRAME);
  EXPECT_EQ(0, tab_helper_.send_count());
}

TEST_F(NetErrorTabHelperTest, NoProbeOnNonMainFrameError) {
  SimulateStart(MAIN_FRAME, NORMAL_PAGE);
  SimulateFail(NOT_MAIN_FRAME, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(0, tab_helper_.probe_count());
}

TEST_F(NetErrorTabHelperTest, ProbeOnDnsError) {
  SimulateMainFrameDnsError();
  EXPECT_EQ(1, tab_helper_.probe_count());
}

TEST_F(NetErrorTabHelperTest, NoProbeWhenNotAllowed) {
  NetErrorTabHelper::set_state_for_testing(
      NetErrorTabHelper::TESTING_FORCE_DISABLED);

  SimulateMainFrameDnsError();
  EXPECT_EQ(0, tab_helper_.probe_count());
}

TEST_F(NetErrorTabHelperTest, HandleProbeResponseBeforeErrorPage) {
  SimulateMainFrameDnsError();
  SimulateMainFrameErrorPageLoad();
  EXPECT_EQ(0, tab_helper_.send_count());

  // Get probe result
  tab_helper_.SimulateProbeResult(kSampleResult);
  EXPECT_EQ(1, tab_helper_.send_count());
  EXPECT_EQ(kSampleResult, tab_helper_.send_result());
}

TEST_F(NetErrorTabHelperTest, HandleProbeResponseAfterErrorPage) {
  SimulateMainFrameDnsError();

  // Get probe result
  tab_helper_.SimulateProbeResult(kSampleResult);

  // Load error page; should send IPC only once it finishes.
  SimulateStart(MAIN_FRAME, ERROR_PAGE);
  SimulateCommit(MAIN_FRAME);
  EXPECT_EQ(0, tab_helper_.send_count());
  SimulateFinish(MAIN_FRAME);
  EXPECT_EQ(1, tab_helper_.send_count());
  EXPECT_EQ(kSampleResult, tab_helper_.send_result());
}

TEST_F(NetErrorTabHelperTest, DiscardObsoleteProbeResponse) {
  SimulateMainFrameDnsError();
  EXPECT_EQ(1, tab_helper_.probe_count());
  SimulateMainFrameErrorPageLoad();
  SimulateStart(MAIN_FRAME, NORMAL_PAGE);
  tab_helper_.SimulateProbeResult(kSampleResult);
  SimulateCommit(MAIN_FRAME);
  SimulateFinish(MAIN_FRAME);
  EXPECT_EQ(0, tab_helper_.send_count());
}

TEST_F(NetErrorTabHelperTest, CoalesceFailures) {
  // We should only start one probe for three failures.
  SimulateMainFrameDnsError();
  SimulateMainFrameDnsError();
  SimulateMainFrameDnsError();
  EXPECT_EQ(1, tab_helper_.probe_count());
  SimulateMainFrameErrorPageLoad();
  tab_helper_.SimulateProbeResult(kSampleResult);
  EXPECT_EQ(1, tab_helper_.send_count());
}

TEST_F(NetErrorTabHelperTest, MultipleProbesWithoutErrorPage) {
  SimulateMainFrameDnsError();
  EXPECT_EQ(1, tab_helper_.probe_count());
  tab_helper_.SimulateProbeResult(kSampleResult);

  // Make sure we can run a new probe after the previous one returned.
  SimulateMainFrameDnsError();
  EXPECT_EQ(2, tab_helper_.probe_count());
}

TEST_F(NetErrorTabHelperTest, MultipleProbesWithErrorPage) {
  SimulateMainFrameDnsError();
  EXPECT_EQ(1, tab_helper_.probe_count());
  tab_helper_.SimulateProbeResult(kSampleResult);
  SimulateMainFrameErrorPageLoad();
  EXPECT_EQ(1, tab_helper_.send_count());
  EXPECT_EQ(kSampleResult, tab_helper_.send_result());

  // Try a different probe result to make sure we send the new one.
  const DnsProbeResult kOtherResult = chrome_common_net::DNS_PROBE_NO_INTERNET;

  // Make sure we can run a new probe after the previous one returned.
  SimulateMainFrameDnsError();
  EXPECT_EQ(2, tab_helper_.probe_count());
  tab_helper_.SimulateProbeResult(kOtherResult);
  SimulateMainFrameErrorPageLoad();
  EXPECT_EQ(2, tab_helper_.send_count());
  EXPECT_EQ(kOtherResult, tab_helper_.send_result());
}

}  // namespace

}  // namespace chrome_browser_net
