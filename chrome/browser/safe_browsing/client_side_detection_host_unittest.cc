// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/task.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock-generated-nice-strict.h"
#include "testing/gmock/include/gmock/gmock-more-actions.h"
#include "testing/gmock/include/gmock/gmock-spec-builders.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace {
const bool kFalse = false;
const bool kTrue = true;
}

namespace safe_browsing {

class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  explicit MockClientSideDetectionService(const FilePath& model_path)
      : ClientSideDetectionService(model_path, NULL) {}
  virtual ~MockClientSideDetectionService() {};

  MOCK_METHOD3(SendClientReportPhishingRequest,
               void(const GURL&, double, ClientReportPhishingRequestCallback*));
  MOCK_CONST_METHOD1(IsPrivateIPAddress, bool(const std::string&));
  MOCK_METHOD2(GetValidCachedResult, bool(const GURL&, bool*));
  MOCK_METHOD1(IsInCache, bool(const GURL&));
  MOCK_METHOD0(OverReportLimit, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSideDetectionService);
};

class MockSafeBrowsingService : public SafeBrowsingService {
 public:
  MockSafeBrowsingService() {}
  virtual ~MockSafeBrowsingService() {}

  MOCK_METHOD8(DisplayBlockingPage,
               void(const GURL&, const GURL&, const std::vector<GURL>&,
                    ResourceType::Type, UrlCheckResult, Client*, int, int));
  MOCK_METHOD1(MatchCsdWhitelistUrl, bool(const GURL&));

  // Helper function which calls OnBlockingPageComplete for this client
  // object.
  void InvokeOnBlockingPageComplete(SafeBrowsingService::Client* client) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(client);
    // Note: this will delete the client object in the case of the CsdClient
    // implementation.
    client->OnBlockingPageComplete(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingService);
};

// Helper function which quits the UI message loop from the IO message loop.
void QuitUIMessageLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          new MessageLoop::QuitTask());
}

class ClientSideDetectionHostTest : public RenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    ui_thread_.reset(new BrowserThread(BrowserThread::UI, &message_loop_));
    // Note: we're starting a real IO thread to make sure our DCHECKs that
    // verify which thread is running are actually tested.
    io_thread_.reset(new BrowserThread(BrowserThread::IO));
    ASSERT_TRUE(io_thread_->Start());

    // Inject service classes.
    ScopedTempDir tmp_dir;
    ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
    FilePath model_path = tmp_dir.path().AppendASCII("model");

    csd_service_.reset(new StrictMock<MockClientSideDetectionService>(
        model_path));
    sb_service_ = new StrictMock<MockSafeBrowsingService>();
    csd_host_ = contents()->safebrowsing_detection_host();
    csd_host_->set_client_side_detection_service(csd_service_.get());
    csd_host_->set_safe_browsing_service(sb_service_.get());
  }

  virtual void TearDown() {
    io_thread_.reset();
    ui_thread_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void OnDetectedPhishingSite(const GURL& phishing_url, double phishing_score) {
    csd_host_->OnDetectedPhishingSite(phishing_url, phishing_score);
  }

  void FlushIOMessageLoop() {
    // If there was a message posted on the IO thread to display the
    // interstitial page we know that it would have been posted before
    // we put the quit message there.
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            NewRunnableFunction(&QuitUIMessageLoop));
    MessageLoop::current()->Run();
  }

  void ExpectPreClassificationChecks(const GURL& url,
                                     const bool* is_private,
                                     const bool* match_csd_whitelist,
                                     const bool* get_valid_cached_result,
                                     const bool* is_in_cache,
                                     const bool* over_report_limit) {
    if (is_private) {
      EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
          .WillOnce(Return(*is_private));
    }
    if (match_csd_whitelist) {
      EXPECT_CALL(*sb_service_, MatchCsdWhitelistUrl(url))
          .WillOnce(Return(*match_csd_whitelist));
    }
    if (get_valid_cached_result) {
      EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(true),
                          Return(*get_valid_cached_result)));
    }
    if (is_in_cache) {
      EXPECT_CALL(*csd_service_, IsInCache(url)).WillOnce(Return(*is_in_cache));
    }
    if (over_report_limit) {
      EXPECT_CALL(*csd_service_, OverReportLimit())
          .WillOnce(Return(*over_report_limit));
    }
  }

  void WaitAndCheckPreClassificationChecks() {
    // Wait for CheckCsdWhitelist to be called if at all.
    FlushIOMessageLoop();
    // Checks for CheckCache() to be called if at all.
    MessageLoop::current()->RunAllPending();
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
  }

 protected:
  ClientSideDetectionHost* csd_host_;
  scoped_ptr<StrictMock<MockClientSideDetectionService> > csd_service_;
  scoped_refptr<StrictMock<MockSafeBrowsingService> > sb_service_;

 private:
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> io_thread_;
};

TEST_F(ClientSideDetectionHostTest, OnDetectedPhishingSiteNotPhishing) {
  // Case 1: client thinks the page is phishing.  The server does not agree.
  // No interstitial is shown.
  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  GURL phishing_url("http://phishingurl.com/");

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(phishing_url, 1.0, _))
      .WillOnce(SaveArg<2>(&cb));
  OnDetectedPhishingSite(phishing_url, 1.0);
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*sb_service_,
              DisplayBlockingPage(_, _, _, _, _, _, _, _)).Times(0);
  cb->Run(phishing_url, false);
  delete cb;
  // If there was a message posted on the IO thread to display the
  // interstitial page we know that it would have been posted before
  // we put the quit message there.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          NewRunnableFunction(&QuitUIMessageLoop));
  MessageLoop::current()->Run();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
}

TEST_F(ClientSideDetectionHostTest, OnDetectedPhishingSiteDisabled) {
  // Case 2: client thinks the page is phishing and so does the server but
  // showing the interstitial is disabled => no interstitial is shown.
  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  GURL phishing_url("http://phishingurl.com/");

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(phishing_url, 1.0, _))
      .WillOnce(SaveArg<2>(&cb));
  OnDetectedPhishingSite(phishing_url, 1.0);
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*sb_service_,
              DisplayBlockingPage(_, _, _, _, _, _, _, _)).Times(0);
  cb->Run(phishing_url, false);
  delete cb;

  FlushIOMessageLoop();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
}

TEST_F(ClientSideDetectionHostTest, OnDetectedPhishingSiteShowInterstitial) {
  // Case 3: client thinks the page is phishing and so does the server.
  // We show an interstitial.
  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  GURL phishing_url("http://phishingurl.com/");

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableClientSidePhishingInterstitial);

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(phishing_url, 1.0, _))
      .WillOnce(SaveArg<2>(&cb));
  OnDetectedPhishingSite(phishing_url, 1.0);
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);

  SafeBrowsingService::Client* client;
  EXPECT_CALL(*sb_service_,
              DisplayBlockingPage(
                  phishing_url,
                  phishing_url,
                  _,
                  ResourceType::MAIN_FRAME,
                  SafeBrowsingService::URL_PHISHING,
                  _ /* a CsdClient object */,
                  contents()->GetRenderProcessHost()->id(),
                  contents()->render_view_host()->routing_id()))
      .WillOnce(SaveArg<5>(&client));

  cb->Run(phishing_url, true);
  delete cb;

  FlushIOMessageLoop();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));

  // Make sure the client object will be deleted.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(
          sb_service_.get(),
          &MockSafeBrowsingService::InvokeOnBlockingPageComplete,
          client));
  // Since the CsdClient object will be deleted on the UI thread I need
  // to run the UI message loop.  Post a task to stop the UI message loop
  // after the client object destructor is called.
  FlushIOMessageLoop();
}

TEST_F(ClientSideDetectionHostTest, OnDetectedPhishingSiteMultiplePings) {
  // Case 4 & 5: client thinks a page is phishing then navigates to
  // another page which is also considered phishing by the client
  // before the server responds with a verdict.  After a while the
  // server responds for both requests with a phishing verdict.  Only
  // a single interstitial is shown for the second URL.
  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  GURL phishing_url("http://phishingurl.com/");

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableClientSidePhishingInterstitial);

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(phishing_url, 1.0, _))
      .WillOnce(SaveArg<2>(&cb));
  OnDetectedPhishingSite(phishing_url, 1.0);
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);
  GURL other_phishing_url("http://other_phishing_url.com/bla");
  ExpectPreClassificationChecks(other_phishing_url, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  // We navigate away.  The callback cb should be revoked.
  NavigateAndCommit(other_phishing_url);
  // Wait for the pre-classification checks to finish for other_phishing_url.
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb_other;
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(other_phishing_url, 0.8, _))
      .WillOnce(SaveArg<2>(&cb_other));
  OnDetectedPhishingSite(other_phishing_url, 0.8);
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb_other);

  // We expect that the interstitial is shown for the second phishing URL and
  // not for the first phishing URL.
  EXPECT_CALL(*sb_service_,
              DisplayBlockingPage(phishing_url, phishing_url,_, _, _, _, _, _))
      .Times(0);
  SafeBrowsingService::Client* client;
  EXPECT_CALL(*sb_service_,
              DisplayBlockingPage(
                  other_phishing_url,
                  other_phishing_url,
                  _,
                  ResourceType::MAIN_FRAME,
                  SafeBrowsingService::URL_PHISHING,
                  _ /* a CsdClient object */,
                  contents()->GetRenderProcessHost()->id(),
                  contents()->render_view_host()->routing_id()))
      .WillOnce(SaveArg<5>(&client));

  cb->Run(phishing_url, true);  // Should have no effect.
  delete cb;
  cb_other->Run(other_phishing_url, true);  // Should show interstitial.
  delete cb_other;

  FlushIOMessageLoop();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));

  // Make sure the client object will be deleted.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(
          sb_service_.get(),
          &MockSafeBrowsingService::InvokeOnBlockingPageComplete,
          client));
  // Since the CsdClient object will be deleted on the UI thread I need
  // to run the UI message loop.  Post a task to stop the UI message loop
  // after the client object destructor is called.
  FlushIOMessageLoop();
}

TEST_F(ClientSideDetectionHostTest, NavigationCancelsShouldClassifyUrl) {
  // Test that canceling pending should classify requests works as expected.

  GURL first_url("http://first.phishy.url.com");
  // The proxy checks is done synchronously so check that it has been done
  // for the first URL.
  ExpectPreClassificationChecks(first_url, &kFalse, &kFalse, NULL, NULL, NULL);
  NavigateAndCommit(first_url);

  // Don't flush the message loop, as we want to navigate to a different
  // url before the final pre-classification checks are run.
  GURL second_url("http://second.url.com/");
  ExpectPreClassificationChecks(second_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(second_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest, ShouldClassifyUrl) {
  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  const IPC::Message* msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_TRUE(msg);
  Tuple1<GURL> actual_url;
  ViewMsg_StartPhishingDetection::Read(msg, &actual_url);
  EXPECT_EQ(url, actual_url.a);
  EXPECT_EQ(rvh()->routing_id(), msg->routing_id());
  process()->sink().ClearMessages();

  // Now try an in-page navigation.  This should not trigger an IPC.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_)).Times(0);
  url = GURL("http://host.com/#foo");
  ExpectPreClassificationChecks(url, NULL, NULL, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // Navigate to a new host, which should cause another IPC.
  url = GURL("http://host2.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_TRUE(msg);
  ViewMsg_StartPhishingDetection::Read(msg, &actual_url);
  EXPECT_EQ(url, actual_url.a);
  EXPECT_EQ(rvh()->routing_id(), msg->routing_id());
  process()->sink().ClearMessages();

  // If IsPrivateIPAddress returns true, no IPC should be triggered.
  url = GURL("http://host3.com/");
  ExpectPreClassificationChecks(url, &kTrue, NULL, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If the connection is proxied, no IPC should be triggered.
  // Note: for this test to work correctly, the new URL must be on the
  // same domain as the previous URL, otherwise it will create a new
  // RenderViewHost that won't have simulate_fetch_via_proxy set.
  url = GURL("http://host3.com/abc");
  rvh()->set_simulate_fetch_via_proxy(true);
  ExpectPreClassificationChecks(url, NULL, NULL, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If the URL is on the csd whitelist, no IPC should be triggered.
  url = GURL("http://host4.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kTrue, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If result is cached, we will try and display the blocking page directly
  // with no start classification message.
  url = GURL("http://host5.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kTrue, NULL,
                                NULL);
  EXPECT_CALL(*sb_service_,
              DisplayBlockingPage(Eq(url), Eq(url), _, _, _, _, _, _))
      .WillOnce(DeleteArg<5>());
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If item is in the cache but it isn't valid, we will classify regardless
  // of whether we are over the reporting limit.
  url = GURL("http://host6.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kTrue,
                                NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_TRUE(msg);
  ViewMsg_StartPhishingDetection::Read(msg, &actual_url);
  EXPECT_EQ(url, actual_url.a);
  EXPECT_EQ(rvh()->routing_id(), msg->routing_id());
  process()->sink().ClearMessages();

  // If the url isn't in the cache and we are over the reporting limit, we
  // don't do classification.
  url = GURL("http://host7.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kTrue);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      ViewMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);
}

}  // namespace safe_browsing
