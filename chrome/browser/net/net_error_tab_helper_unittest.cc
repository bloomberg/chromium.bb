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
        probe_start_count_(0),
        probes_allowed_(true) {
  }

  // NetErrorTabHelper implementation:

  virtual void PostStartDnsProbeTask() OVERRIDE {
    ++probe_start_count_;
  }

  virtual bool ProbesAllowed() const OVERRIDE {
    return probes_allowed_;
  }

  // Methods to control mock behavior and verify things:

  void set_probes_allowed(bool probes_allowed) {
    probes_allowed_ = probes_allowed;
  }

  void SimulateProbeResult(DnsProbeResult result) {
    OnDnsProbeFinished(result);
  }

  bool dns_probe_running() {
    return NetErrorTabHelper::dns_probe_running();
  }

  int probe_start_count() {
    return probe_start_count_;
  }

 private:
  int probe_start_count_;
  bool probes_allowed_;
};

class NetErrorTabHelperTest : public testing::Test {
 public:
  NetErrorTabHelperTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  void SimulateFailure(bool is_main_frame, int error_code) {
    GURL url;
    string16 error_description;

    tab_helper_.DidFailProvisionalLoad(
        1 /* frame_id */,
        is_main_frame,
        url,
        error_code,
        error_description,
        NULL /* render_view_host */);
  }

  MessageLoop message_loop_;
  TestBrowserThread ui_thread_;

  TestNetErrorTabHelper tab_helper_;
};

TEST_F(NetErrorTabHelperTest, InitialState) {
  EXPECT_FALSE(tab_helper_.dns_probe_running());
  EXPECT_EQ(0, tab_helper_.probe_start_count());
}

TEST_F(NetErrorTabHelperTest, NoProbeOnNonDnsError) {
  SimulateFailure(true, net::ERR_FAILED);
  EXPECT_FALSE(tab_helper_.dns_probe_running());
  EXPECT_EQ(0, tab_helper_.probe_start_count());
}

TEST_F(NetErrorTabHelperTest, NoProbeOnNonMainFrameError) {
  SimulateFailure(false, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_FALSE(tab_helper_.dns_probe_running());
  EXPECT_EQ(0, tab_helper_.probe_start_count());
}

TEST_F(NetErrorTabHelperTest, ProbeOnDnsError) {
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(tab_helper_.dns_probe_running());
  EXPECT_EQ(1, tab_helper_.probe_start_count());
}

TEST_F(NetErrorTabHelperTest, NoProbeWhenNotAllowed) {
  tab_helper_.set_probes_allowed(false);
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_FALSE(tab_helper_.dns_probe_running());
  EXPECT_EQ(0, tab_helper_.probe_start_count());
}

TEST_F(NetErrorTabHelperTest, CoalesceFailures) {
  // We should only start one probe for three failures.
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(tab_helper_.dns_probe_running());
  EXPECT_EQ(1, tab_helper_.probe_start_count());
}

TEST_F(NetErrorTabHelperTest, HandleProbeResponse) {
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);

  // Make sure we handle probe response.
  tab_helper_.SimulateProbeResult(chrome_common_net::DNS_PROBE_UNKNOWN);
  EXPECT_FALSE(tab_helper_.dns_probe_running());
  EXPECT_EQ(1, tab_helper_.probe_start_count());
}

TEST_F(NetErrorTabHelperTest, MultipleProbes) {
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);
  tab_helper_.SimulateProbeResult(chrome_common_net::DNS_PROBE_UNKNOWN);

  // Make sure we can run a new probe after the previous one returned.
  SimulateFailure(true, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(tab_helper_.dns_probe_running());
  EXPECT_EQ(2, tab_helper_.probe_start_count());

  tab_helper_.SimulateProbeResult(chrome_common_net::DNS_PROBE_UNKNOWN);
  EXPECT_FALSE(tab_helper_.dns_probe_running());
  EXPECT_EQ(2, tab_helper_.probe_start_count());
}

}  // namespace

}  // namespace chrome_browser_net
