// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/common/safebrowsing_messages.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using content::BrowserThread;
using content::RenderFrameHostTester;
using content::WebContents;

namespace {

const bool kFalse = false;
const bool kTrue = true;

}  // namespace

namespace safe_browsing {
namespace {
// This matcher verifies that the client computed verdict
// (ClientPhishingRequest) which is passed to SendClientReportPhishingRequest
// has the expected fields set.  Note: we can't simply compare the protocol
// buffer strings because the BrowserFeatureExtractor might add features to the
// verdict object before calling SendClientReportPhishingRequest.
MATCHER_P(PartiallyEqualVerdict, other, "") {
  return (other.url() == arg.url() &&
          other.client_score() == arg.client_score() &&
          other.is_phishing() == arg.is_phishing());
}

MATCHER_P(PartiallyEqualMalwareVerdict, other, "") {
  if (other.url() != arg.url() ||
      other.referrer_url() != arg.referrer_url() ||
      other.bad_ip_url_info_size() != arg.bad_ip_url_info_size())
    return false;

  for (int i = 0; i < other.bad_ip_url_info_size(); ++i) {
    if (other.bad_ip_url_info(i).ip() != arg.bad_ip_url_info(i).ip() ||
        other.bad_ip_url_info(i).url() != arg.bad_ip_url_info(i).url())
    return false;
  }
  return true;
}

// Test that the callback is NULL when the verdict is not phishing.
MATCHER(CallbackIsNull, "") {
  return arg.is_null();
}

ACTION(QuitUIMessageLoop) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::MessageLoopForUI::current()->QuitWhenIdle();
}

ACTION_P(InvokeDoneCallback, verdict) {
  std::unique_ptr<ClientPhishingRequest> request(::std::tr1::get<1>(args));
  request->CopyFrom(*verdict);
  ::std::tr1::get<2>(args).Run(true, std::move(request));
}

ACTION_P(InvokeMalwareCallback, verdict) {
  std::unique_ptr<ClientMalwareRequest> request(::std::tr1::get<1>(args));
  request->CopyFrom(*verdict);
  ::std::tr1::get<2>(args).Run(true, std::move(request));
}

void EmptyUrlCheckCallback(bool processed) {
}

class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService() : ClientSideDetectionService(NULL) {}
  virtual ~MockClientSideDetectionService() {}

  MOCK_METHOD3(SendClientReportPhishingRequest,
               void(ClientPhishingRequest*,
                    bool,
                    const ClientReportPhishingRequestCallback&));
  MOCK_METHOD2(SendClientReportMalwareRequest,
               void(ClientMalwareRequest*,
                    const ClientReportMalwareRequestCallback&));
  MOCK_CONST_METHOD1(IsPrivateIPAddress, bool(const std::string&));
  MOCK_METHOD2(GetValidCachedResult, bool(const GURL&, bool*));
  MOCK_METHOD1(IsInCache, bool(const GURL&));
  MOCK_METHOD0(OverPhishingReportLimit, bool());
  MOCK_METHOD0(OverMalwareReportLimit, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSideDetectionService);
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  explicit MockSafeBrowsingUIManager(SafeBrowsingService* service)
      : SafeBrowsingUIManager(service) { }

  MOCK_METHOD1(DisplayBlockingPage, void(const UnsafeResource& resource));

  // Helper function which calls OnBlockingPageComplete for this client
  // object.
  void InvokeOnBlockingPageComplete(
    const security_interstitials::UnsafeResource::UrlCheckCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // Note: this will delete the client object in the case of the CsdClient
    // implementation.
    if (!callback.is_null())
      callback.Run(false);
  }

 protected:
  virtual ~MockSafeBrowsingUIManager() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(MatchCsdWhitelistUrl, bool(const GURL&));
  MOCK_METHOD1(MatchMalwareIP, bool(const std::string& ip_address));
  MOCK_METHOD0(IsMalwareKillSwitchOn, bool());

 protected:
  virtual ~MockSafeBrowsingDatabaseManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class MockTestingProfile : public TestingProfile {
 public:
  MockTestingProfile() {}
  virtual ~MockTestingProfile() {}

  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
};

class MockBrowserFeatureExtractor : public BrowserFeatureExtractor {
 public:
  explicit MockBrowserFeatureExtractor(
      WebContents* tab,
      ClientSideDetectionHost* host)
      : BrowserFeatureExtractor(tab, host) {}
  virtual ~MockBrowserFeatureExtractor() {}

  MOCK_METHOD3(ExtractFeatures,
               void(const BrowseInfo*,
                    ClientPhishingRequest*,
                    const BrowserFeatureExtractor::DoneCallback&));

  MOCK_METHOD3(ExtractMalwareFeatures,
               void(BrowseInfo*,
                    ClientMalwareRequest*,
                    const BrowserFeatureExtractor::MalwareDoneCallback&));
};

}  // namespace

class ClientSideDetectionHostTest : public ChromeRenderViewHostTestHarness {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Inject service classes.
    csd_service_.reset(new StrictMock<MockClientSideDetectionService>());
    database_manager_ = new StrictMock<MockSafeBrowsingDatabaseManager>();
    ui_manager_ = new StrictMock<MockSafeBrowsingUIManager>(
        SafeBrowsingService::CreateSafeBrowsingService());
    csd_host_.reset(ClientSideDetectionHost::Create(web_contents()));
    csd_host_->set_client_side_detection_service(csd_service_.get());
    csd_host_->set_safe_browsing_managers(ui_manager_.get(),
                                          database_manager_.get());
    // We need to create this here since we don't call DidStopLanding in
    // this test.
    csd_host_->browse_info_.reset(new BrowseInfo);
  }

  void TearDown() override {
    // Delete the host object on the UI thread and release the
    // SafeBrowsingService.
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE,
                              csd_host_.release());
    database_manager_ = NULL;
    ui_manager_ = NULL;
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::BrowserContext* CreateBrowserContext() override {
    // Set custom profile object so that we can mock calls to IsOffTheRecord.
    // This needs to happen before we call the parent SetUp() function.  We use
    // a nice mock because other parts of the code are calling IsOffTheRecord.
    mock_profile_ = new NiceMock<MockTestingProfile>();
    return mock_profile_;
  }

  void OnPhishingDetectionDone(const std::string& verdict_str) {
    csd_host_->OnPhishingDetectionDone(verdict_str);
  }

  void DidStopLoading() { csd_host_->DidStopLoading(); }

  void UpdateIPUrlMap(const std::string& ip, const std::string& host) {
    csd_host_->UpdateIPUrlMap(ip, host, "", "", content::RESOURCE_TYPE_OBJECT);
  }

  BrowseInfo* GetBrowseInfo() {
    return csd_host_->browse_info_.get();
  }

  void ExpectPreClassificationChecks(const GURL& url,
                                     const bool* is_private,
                                     const bool* is_incognito,
                                     const bool* match_csd_whitelist,
                                     const bool* malware_killswitch,
                                     const bool* get_valid_cached_result,
                                     const bool* is_in_cache,
                                     const bool* over_phishing_report_limit,
                                     const bool* over_malware_report_limit) {
    if (is_private) {
      EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
          .WillOnce(Return(*is_private));
    }
    if (is_incognito) {
      EXPECT_CALL(*mock_profile_, IsOffTheRecord())
          .WillRepeatedly(Return(*is_incognito));
    }
    if (match_csd_whitelist) {
      EXPECT_CALL(*database_manager_.get(), MatchCsdWhitelistUrl(url))
          .WillOnce(Return(*match_csd_whitelist));
    }
    if (malware_killswitch) {
      EXPECT_CALL(*database_manager_.get(), IsMalwareKillSwitchOn())
          .WillRepeatedly(Return(*malware_killswitch));
    }
    if (get_valid_cached_result) {
      EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(true),
                          Return(*get_valid_cached_result)));
    }
    if (is_in_cache) {
      EXPECT_CALL(*csd_service_, IsInCache(url)).WillOnce(Return(*is_in_cache));
    }
    if (over_phishing_report_limit) {
      EXPECT_CALL(*csd_service_, OverPhishingReportLimit())
          .WillOnce(Return(*over_phishing_report_limit));
    }
    if (over_malware_report_limit) {
      EXPECT_CALL(*csd_service_, OverMalwareReportLimit())
          .WillOnce(Return(*over_malware_report_limit));
    }
  }

  void WaitAndCheckPreClassificationChecks() {
    // Wait for CheckCsdWhitelist and CheckCache() to be called if at all.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(database_manager_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(mock_profile_));
  }

  void SetFeatureExtractor(BrowserFeatureExtractor* extractor) {
    csd_host_->feature_extractor_.reset(extractor);
  }

  void SetRedirectChain(const std::vector<GURL>& redirect_chain) {
    csd_host_->browse_info_->url_redirects = redirect_chain;
  }

  void SetReferrer(const GURL& referrer) {
    csd_host_->browse_info_->referrer = referrer;
  }

  void ExpectShouldClassifyForMalwareResult(bool should_classify) {
    EXPECT_EQ(should_classify, csd_host_->should_classify_for_malware_);
  }

  void ExpectStartPhishingDetection(const GURL* url) {
    const IPC::Message* msg = process()->sink().GetFirstMessageMatching(
        SafeBrowsingMsg_StartPhishingDetection::ID);
    if (url) {
      ASSERT_TRUE(msg);
      std::tuple<GURL> actual_url;
      SafeBrowsingMsg_StartPhishingDetection::Read(msg, &actual_url);
      EXPECT_EQ(*url, std::get<0>(actual_url));
      EXPECT_EQ(main_rfh()->GetRoutingID(), msg->routing_id());
      process()->sink().ClearMessages();
    } else {
      ASSERT_FALSE(msg);
    }
  }

  void TestUnsafeResourceCopied(const UnsafeResource& resource) {
    ASSERT_TRUE(csd_host_->unsafe_resource_.get());
    // Test that the resource from OnSafeBrowsingHit notification was copied
    // into the CSDH.
    EXPECT_EQ(resource.url, csd_host_->unsafe_resource_->url);
    EXPECT_EQ(resource.original_url, csd_host_->unsafe_resource_->original_url);
    EXPECT_EQ(resource.is_subresource,
              csd_host_->unsafe_resource_->is_subresource);
    EXPECT_EQ(resource.threat_type, csd_host_->unsafe_resource_->threat_type);
    EXPECT_TRUE(csd_host_->unsafe_resource_->callback.is_null());
    EXPECT_EQ(resource.web_contents_getter.Run(),
              csd_host_->unsafe_resource_->web_contents_getter.Run());
  }

  void SetUnsafeSubResourceForCurrent(bool expect_unsafe_resource) {
    UnsafeResource resource;
    resource.url = GURL("http://www.malware.com/");
    resource.original_url = web_contents()->GetURL();
    resource.is_subresource = true;
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    resource.callback = base::Bind(&EmptyUrlCheckCallback);
    resource.callback_thread =
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
    resource.web_contents_getter =
        SafeBrowsingUIManager::UnsafeResource::GetWebContentsGetter(
            web_contents()->GetRenderProcessHost()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID());
    csd_host_->OnSafeBrowsingHit(resource);
    resource.callback.Reset();
    ASSERT_EQ(expect_unsafe_resource, csd_host_->DidShowSBInterstitial());
    if (expect_unsafe_resource)
      TestUnsafeResourceCopied(resource);
  }

  void NavigateWithSBHitAndCommit(const GURL& url) {
    // Create a pending navigation.
    controller().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

    ASSERT_TRUE(pending_main_rfh());
    if (web_contents()->GetMainFrame()->GetProcess()->GetID() ==
        pending_main_rfh()->GetProcess()->GetID()) {
      EXPECT_NE(web_contents()->GetMainFrame()->GetRoutingID(),
                pending_main_rfh()->GetRoutingID());
    }

    // Simulate a safebrowsing hit before navigation completes.
    UnsafeResource resource;
    resource.url = url;
    resource.original_url = url;
    resource.is_subresource = false;
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    resource.callback = base::Bind(&EmptyUrlCheckCallback);
    resource.callback_thread =
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
    resource.web_contents_getter =
        SafeBrowsingUIManager::UnsafeResource::GetWebContentsGetter(
            pending_rvh()->GetProcess()->GetID(),
            pending_main_rfh()->GetRoutingID());
    csd_host_->OnSafeBrowsingHit(resource);
    resource.callback.Reset();

    // LoadURL created a navigation entry, now simulate the RenderView sending
    // a notification that it actually navigated.
    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

    ASSERT_TRUE(csd_host_->DidShowSBInterstitial());
    TestUnsafeResourceCopied(resource);
  }

  void NavigateWithoutSBHitAndCommit(const GURL& safe_url) {
    controller().LoadURL(
        safe_url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
        std::string());

    ASSERT_TRUE(pending_main_rfh());
    if (web_contents()->GetMainFrame()->GetProcess()->GetID() ==
        pending_main_rfh()->GetProcess()->GetID()) {
      EXPECT_NE(web_contents()->GetMainFrame()->GetRoutingID(),
                pending_main_rfh()->GetRoutingID());
    }

    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
    ASSERT_FALSE(csd_host_->DidShowSBInterstitial());
  }

  void CheckIPUrlEqual(const std::vector<IPUrlInfo>& expect,
                       const std::vector<IPUrlInfo>& result) {
    ASSERT_EQ(expect.size(), result.size());

    for (unsigned int i = 0; i < expect.size(); ++i) {
      EXPECT_EQ(expect[i].url, result[i].url);
      EXPECT_EQ(expect[i].method, result[i].method);
      EXPECT_EQ(expect[i].referrer, result[i].referrer);
      EXPECT_EQ(expect[i].resource_type, result[i].resource_type);
    }
  }

 protected:
  std::unique_ptr<ClientSideDetectionHost> csd_host_;
  std::unique_ptr<StrictMock<MockClientSideDetectionService>> csd_service_;
  scoped_refptr<StrictMock<MockSafeBrowsingUIManager> > ui_manager_;
  scoped_refptr<StrictMock<MockSafeBrowsingDatabaseManager> > database_manager_;
  MockTestingProfile* mock_profile_;  // We don't own this object
};

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneInvalidVerdict) {
  // Case 0: renderer sends an invalid verdict string that we're unable to
  // parse.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(
          web_contents(),
          csd_host_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.
  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
  OnPhishingDetectionDone("Invalid Protocol Buffer");
  EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneNotPhishing) {
  // Case 1: client thinks the page is phishing.  The server does not agree.
  // No interstitial is shown.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(
          web_contents(),
          csd_host_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(InvokeDoneCallback(&verdict));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, _))
      .WillOnce(DoAll(DeleteArg<0>(), SaveArg<2>(&cb)));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  ASSERT_FALSE(cb.is_null());

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);
  cb.Run(GURL(verdict.url()), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneDisabled) {
  // Case 2: client thinks the page is phishing and so does the server but
  // showing the interstitial is disabled => no interstitial is shown.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(
          web_contents(),
          csd_host_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(InvokeDoneCallback(&verdict));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, _))
      .WillOnce(DoAll(DeleteArg<0>(), SaveArg<2>(&cb)));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  ASSERT_FALSE(cb.is_null());

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);
  cb.Run(GURL(verdict.url()), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneShowInterstitial) {
  // Case 3: client thinks the page is phishing and so does the server.
  // We show an interstitial.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(
          web_contents(),
          csd_host_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(InvokeDoneCallback(&verdict));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, _))
      .WillOnce(DoAll(DeleteArg<0>(), SaveArg<2>(&cb)));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));
  cb.Run(phishing_url, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(phishing_url, resource.url);
  EXPECT_EQ(phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL, resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(), resource.web_contents_getter.Run());

  // Make sure the client object will be deleted.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                 ui_manager_, resource.callback));
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneMultiplePings) {
  // Case 4 & 5: client thinks a page is phishing then navigates to
  // another page which is also considered phishing by the client
  // before the server responds with a verdict.  After a while the
  // server responds for both requests with a phishing verdict.  Only
  // a single interstitial is shown for the second URL.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(
          web_contents(),
          csd_host_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(InvokeDoneCallback(&verdict));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, _))
      .WillOnce(DoAll(DeleteArg<0>(), SaveArg<2>(&cb)));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  // Set this back to a normal browser feature extractor since we're using
  // NavigateAndCommit() and it's easier to use the real thing than setting up
  // mock expectations.
  SetFeatureExtractor(new BrowserFeatureExtractor(web_contents(),
                                                  csd_host_.get()));
  GURL other_phishing_url("http://other_phishing_url.com/bla");
  ExpectPreClassificationChecks(other_phishing_url, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse, &kFalse);
  // We navigate away.  The callback cb should be revoked.
  NavigateAndCommit(other_phishing_url);
  // Wait for the pre-classification checks to finish for other_phishing_url.
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb_other;
  verdict.set_url(other_phishing_url.spec());
  verdict.set_client_score(0.8f);
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, _))
      .WillOnce(DoAll(DeleteArg<0>(),
                      SaveArg<2>(&cb_other),
                      QuitUIMessageLoop()));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(other_phishing_url);
  SetRedirectChain(redirect_chain);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  base::RunLoop().Run();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb_other.is_null());

  // We expect that the interstitial is shown for the second phishing URL and
  // not for the first phishing URL.
  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  cb.Run(phishing_url, true);  // Should have no effect.
  cb_other.Run(other_phishing_url, true);  // Should show interstitial.

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(other_phishing_url, resource.url);
  EXPECT_EQ(other_phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL, resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(), resource.web_contents_getter.Run());

  // Make sure the client object will be deleted.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                 ui_manager_, resource.callback));
}

TEST_F(ClientSideDetectionHostTest,
       OnPhishingDetectionDoneVerdictNotPhishing) {
  // Case 6: renderer sends a verdict string that isn't phishing.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(
          web_contents(),
          csd_host_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientPhishingRequest verdict;
  verdict.set_url("http://not-phishing.com/");
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));
}

TEST_F(ClientSideDetectionHostTest,
       OnPhishingDetectionDoneVerdictNotPhishingButSBMatchSubResource) {
  // Case 7: renderer sends a verdict string that isn't phishing but the URL
  // of a subresource was on the regular phishing or malware lists.
  GURL url("http://not-phishing.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  // First we have to navigate to the URL to set the unique page ID.
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  SetUnsafeSubResourceForCurrent(true /* expect_unsafe_resource */);

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, CallbackIsNull()))
      .WillOnce(DoAll(DeleteArg<0>(), QuitUIMessageLoop()));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  base::RunLoop().Run();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
}

TEST_F(ClientSideDetectionHostTest,
       OnPhishingDetectionDoneVerdictNotPhishingButSBMatchOnNewRVH) {
  // When navigating to a different host (thus creating a pending RVH) which
  // matches regular malware list, and after navigation the renderer sends a
  // verdict string that isn't phishing, we should still send the report.

  // Do an initial navigation to a safe host.
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(
      start_url, &kFalse, &kFalse, &kFalse, &kFalse, &kFalse, &kFalse, &kFalse,
      &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();

  // Now navigate to a different host which will have a malware hit before the
  // navigation commits.
  GURL url("http://malware-but-not-phishing.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateWithSBHitAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, CallbackIsNull()))
      .WillOnce(DoAll(DeleteArg<0>(), QuitUIMessageLoop()));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  base::RunLoop().Run();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));

  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateWithoutSBHitAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(
    ClientSideDetectionHostTest,
    OnPhishingDetectionDoneVerdictNotPhishingButSBMatchOnSubresourceWhileNavPending) {
  // When a malware hit happens on a committed page while a slow pending load is
  // in progress, the csd report should be sent for the committed page.

  // Do an initial navigation to a safe host.
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(
      start_url, &kFalse, &kFalse, &kFalse, &kFalse, &kFalse, &kFalse, &kFalse,
      &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();

  // Now navigate to a different host which does not have a SB hit.
  GURL url("http://not-malware-not-phishing-but-malware-subresource.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateWithoutSBHitAndCommit(url);

  // Simulate a subresource malware hit on committed page.
  SetUnsafeSubResourceForCurrent(true /* expect_unsafe_resource */);

  // Create a pending navigation, but don't commit it.
  GURL pending_url("http://slow.example.com/");
  content::WebContentsTester::For(web_contents())->StartNavigation(pending_url);

  WaitAndCheckPreClassificationChecks();

  // Even though we have a pending navigation, the DidShowSBInterstitial check
  // should apply to the committed navigation, so we should get a report even
  // though the verdict has is_phishing = false.
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _, CallbackIsNull()))
      .WillOnce(DoAll(DeleteArg<0>(), QuitUIMessageLoop()));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  base::RunLoop().Run();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
}

TEST_F(ClientSideDetectionHostTest, SafeBrowsingHitOnFreshTab) {
  // A fresh WebContents should not have any NavigationEntries yet. (See
  // https://crbug.com/524208.)
  EXPECT_EQ(nullptr, controller().GetLastCommittedEntry());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());

  // Simulate a subresource malware hit (this could happen if the WebContents
  // was created with window.open, and had content injected into it).
  SetUnsafeSubResourceForCurrent(false /* expect_unsafe_resource */);

  // If the test didn't crash, we're good. (Nothing else to test, since there
  // was no DidNavigateMainFrame, CSD won't do anything with this hit.)
}

TEST_F(ClientSideDetectionHostTest,
       DidStopLoadingShowMalwareInterstitial) {
  // Case 9: client thinks the page match malware IP and so does the server.
  // We show an sub-resource malware interstitial.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(
          web_contents(),
          csd_host_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  GURL malware_landing_url("http://malware.com/");
  GURL malware_ip_url("http://badip.com");
  ClientMalwareRequest malware_verdict;
  malware_verdict.set_url("http://malware.com/");
  ClientMalwareRequest::UrlInfo* badipurl =
      malware_verdict.add_bad_ip_url_info();
  badipurl->set_ip("1.2.3.4");
  badipurl->set_url("http://badip.com");

  ExpectPreClassificationChecks(GURL(malware_verdict.url()), &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(GURL(malware_verdict.url()));
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportMalwareRequestCallback cb;
  EXPECT_CALL(*mock_extractor, ExtractMalwareFeatures(_, _, _))
      .WillOnce(InvokeMalwareCallback(&malware_verdict));
  EXPECT_CALL(*csd_service_,
              SendClientReportMalwareRequest(
                  Pointee(PartiallyEqualMalwareVerdict(malware_verdict)), _))
      .WillOnce(DoAll(DeleteArg<0>(), SaveArg<1>(&cb)));
  DidStopLoading();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));
  cb.Run(malware_landing_url, malware_ip_url, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(malware_ip_url, resource.url);
  EXPECT_EQ(malware_landing_url, resource.original_url);
  EXPECT_TRUE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL, resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(), resource.web_contents_getter.Run());

  // Make sure the client object will be deleted.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                 ui_manager_, resource.callback));
}

TEST_F(ClientSideDetectionHostTest, UpdateIPUrlMap) {
  BrowseInfo* browse_info = GetBrowseInfo();

  // Empty IP or host are skipped
  UpdateIPUrlMap("250.10.10.10", std::string());
  ASSERT_EQ(0U, browse_info->ips.size());
  UpdateIPUrlMap(std::string(), "http://google.com/a");
  ASSERT_EQ(0U, browse_info->ips.size());
  UpdateIPUrlMap(std::string(), std::string());
  ASSERT_EQ(0U, browse_info->ips.size());

  std::vector<IPUrlInfo> expected_urls;
  for (int i = 0; i < 20; i++) {
    std::string url = base::StringPrintf("http://%d.com/", i);
    expected_urls.push_back(
        IPUrlInfo(url, "", "", content::RESOURCE_TYPE_OBJECT));
    UpdateIPUrlMap("250.10.10.10", url);
  }
  ASSERT_EQ(1U, browse_info->ips.size());
  ASSERT_EQ(20U, browse_info->ips["250.10.10.10"].size());
  CheckIPUrlEqual(expected_urls,
                  browse_info->ips["250.10.10.10"]);

  // Add more urls for this ip, it exceeds max limit and won't be added
  UpdateIPUrlMap("250.10.10.10", "http://21.com/");
  ASSERT_EQ(1U, browse_info->ips.size());
  ASSERT_EQ(20U, browse_info->ips["250.10.10.10"].size());
  CheckIPUrlEqual(expected_urls,
                  browse_info->ips["250.10.10.10"]);

  // Add 199 more IPs
  for (int i = 0; i < 199; i++) {
    std::string ip = base::StringPrintf("%d.%d.%d.256", i, i, i);
    expected_urls.clear();
    expected_urls.push_back(
        IPUrlInfo("test.com/", "", "", content::RESOURCE_TYPE_OBJECT));
    UpdateIPUrlMap(ip, "test.com/");
    ASSERT_EQ(1U, browse_info->ips[ip].size());
    CheckIPUrlEqual(expected_urls,
                    browse_info->ips[ip]);
  }
  ASSERT_EQ(200U, browse_info->ips.size());

  // Exceeding max ip limit 200, these won't be added
  UpdateIPUrlMap("250.250.250.250", "goo.com/");
  UpdateIPUrlMap("250.250.250.250", "bar.com/");
  UpdateIPUrlMap("250.250.0.250", "foo.com/");
  ASSERT_EQ(200U, browse_info->ips.size());

  // Add url to existing IPs succeed
  UpdateIPUrlMap("100.100.100.256", "more.com/");
  ASSERT_EQ(2U, browse_info->ips["100.100.100.256"].size());
  expected_urls.clear();
  expected_urls.push_back(
      IPUrlInfo("test.com/", "", "", content::RESOURCE_TYPE_OBJECT));
  expected_urls.push_back(
      IPUrlInfo("more.com/", "", "", content::RESOURCE_TYPE_OBJECT));
  CheckIPUrlEqual(expected_urls,
                  browse_info->ips["100.100.100.256"]);
}

TEST_F(ClientSideDetectionHostTest, NavigationCancelsShouldClassifyUrl) {
  // Test that canceling pending should classify requests works as expected.

  GURL first_url("http://first.phishy.url.com");
  GURL second_url("http://second.url.com/");
  // The first few checks are done synchronously so check that they have been
  // done for the first URL, while the second URL has all the checks done.  We
  // need to manually set up the IsPrivateIPAddress mock since if the same mock
  // expectation is specified twice, gmock will only use the last instance of
  // it, meaning the first will never be matched.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  ExpectPreClassificationChecks(first_url, NULL, &kFalse, &kFalse, &kFalse,
                                NULL, NULL, NULL, NULL);
  ExpectPreClassificationChecks(second_url, NULL, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);

  NavigateAndCommit(first_url);
  // Don't flush the message loop, as we want to navigate to a different
  // url before the final pre-classification checks are run.
  NavigateAndCommit(second_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckPass) {
  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckInPageNavigation) {
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url);
  ExpectShouldClassifyForMalwareResult(true);

  // Now try an in-page navigation.  This should not trigger an IPC.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_)).Times(0);
  GURL inpage("http://host.com/#foo");
  ExpectPreClassificationChecks(inpage, NULL, NULL, NULL, NULL, NULL, NULL,
                                NULL, NULL);
  NavigateAndCommit(inpage);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckXHTML) {
  // Check that XHTML is supported, in addition to the default HTML type.
  GURL url("http://host.com/xhtml");
  RenderFrameHostTester::For(web_contents()->GetMainFrame())->
      SetContentsMimeType("application/xhtml+xml");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckTwoNavigations) {
  // Navigate to two hosts, which should cause two IPCs.
  GURL url1("http://host1.com/");
  ExpectPreClassificationChecks(url1, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url1);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url1);
  ExpectShouldClassifyForMalwareResult(true);

  GURL url2("http://host2.com/");
  ExpectPreClassificationChecks(url2, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url2);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url2);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckMimeType) {
  // If the mime type is not one that we support, no IPC should be triggered
  // but all pre-classification checks should run because we might classify
  // other mime types for malware.
  // Note: for this test to work correctly, the new URL must be on the
  // same domain as the previous URL, otherwise it will create a new
  // RenderFrameHost that won't have the mime type set.
  GURL url("http://host2.com/image.jpg");
  RenderFrameHostTester::For(web_contents()->GetMainFrame())->
      SetContentsMimeType("image/jpeg");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckPrivateIpAddress) {
  // If IsPrivateIPAddress returns true, no IPC should be triggered.
  GURL url("http://host3.com/");
  ExpectPreClassificationChecks(url, &kTrue, &kFalse, NULL, NULL, NULL, NULL,
                                NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  const IPC::Message* msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);
  ExpectShouldClassifyForMalwareResult(false);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckIncognito) {
  // If the tab is incognito there should be no IPC.  Also, we shouldn't
  // even check the csd-whitelist.
  GURL url("http://host4.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kTrue, NULL, NULL, NULL, NULL,
                                NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(false);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckCsdWhitelist) {
  // If the URL is on the csd whitelist no phishing IPC should be sent
  // but we should classify the URL for malware.
  GURL url("http://host5.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kTrue, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckMalwareKillSwitch) {
  // If the malware killswitch is on we shouldn't classify the page for malware.
  GURL url("http://host5.com/kill-switch");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kTrue, &kFalse,
                                &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url);
  ExpectShouldClassifyForMalwareResult(false);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckKillswitchAndCsdWhitelist) {
  // If both the malware kill-swtich is on and the URL is on the csd whitelist,
  // we will leave pre-classification checks early.
  GURL url("http://host5.com/kill-switch-and-whitelisted");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kTrue, &kTrue, NULL,
                                NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(false);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckInvalidCache) {
  // If item is in the cache but it isn't valid, we will classify regardless
  // of whether we are over the reporting limit.
  GURL url("http://host6.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kTrue, NULL, &kFalse);

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverPhishingReportingLimit) {
  // If the url isn't in the cache and we are over the reporting limit, we
  // don't do classification.
  GURL url("http://host7.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kTrue, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverMalwareReportingLimit) {
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kTrue);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(&url);
  ExpectShouldClassifyForMalwareResult(false);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverBothReportingLimits) {
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kTrue, &kTrue);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(false);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckHttpsUrl) {
  GURL url("https://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectStartPhishingDetection(NULL);
  ExpectShouldClassifyForMalwareResult(true);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckValidCached) {
  // If result is cached, we will try and display the blocking page directly
  // with no start classification message.
  GURL url("http://host8.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse, &kTrue,
                                &kFalse, &kFalse, &kFalse);

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  EXPECT_EQ(url, resource.url);
  EXPECT_EQ(url, resource.original_url);

  ExpectStartPhishingDetection(NULL);

  // Showing a phishing warning will invalidate all the weak pointers which
  // means we will not extract malware features.
  ExpectShouldClassifyForMalwareResult(false);
}
}  // namespace safe_browsing
