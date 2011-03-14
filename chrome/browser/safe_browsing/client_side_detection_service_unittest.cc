// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/csd.pb.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    file_thread_.reset(new BrowserThread(BrowserThread::FILE, &msg_loop_));

    factory_.reset(new FakeURLFetcherFactory());
    URLFetcher::set_factory(factory_.get());

    browser_thread_.reset(new BrowserThread(BrowserThread::UI, &msg_loop_));
  }

  virtual void TearDown() {
    msg_loop_.RunAllPending();
    csd_service_.reset();
    URLFetcher::set_factory(NULL);
    file_thread_.reset();
    browser_thread_.reset();
  }

  base::PlatformFile GetModelFile() {
    model_file_ = base::kInvalidPlatformFileValue;
    csd_service_->GetModelFile(NewCallback(
        this, &ClientSideDetectionServiceTest::GetModelFileDone));
    // This method will block this thread until GetModelFileDone is called.
    msg_loop_.Run();
    return model_file_;
  }

  std::string ReadModelFile(base::PlatformFile model_file) {
    char buf[1024];
    int n = base::ReadPlatformFile(model_file, 0, buf, 1024);
    EXPECT_LE(0, n);
    return (n < 0) ? "" : std::string(buf, n);
  }

  bool SendClientReportPhishingRequest(const GURL& phishing_url,
                                       double score) {
    csd_service_->SendClientReportPhishingRequest(
        phishing_url,
        score,
        NewCallback(this, &ClientSideDetectionServiceTest::SendRequestDone));
    phishing_url_ = phishing_url;
    msg_loop_.Run();  // Waits until callback is called.
    return is_phishing_;
  }

  void SetModelFetchResponse(std::string response_data, bool success) {
    factory_->SetFakeResponse(ClientSideDetectionService::kClientModelUrl,
                              response_data, success);
  }

  void SetClientReportPhishingResponse(std::string response_data,
                                       bool success) {
    factory_->SetFakeResponse(
        ClientSideDetectionService::kClientReportPhishingUrl,
        response_data, success);
  }

  int GetNumReports() {
    return csd_service_->GetNumReports();
  }

  std::queue<base::Time>& GetPhishingReportTimes() {
    return csd_service_->phishing_report_times_;
  }

  void SetCache(const GURL& gurl, bool is_phishing, base::Time time) {
    csd_service_->cache_[gurl] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(is_phishing,
                                                                   time));
  }

  void TestCache() {
    ClientSideDetectionService::PhishingCache& cache = csd_service_->cache_;
    base::Time now = base::Time::Now();
    base::Time time = now - ClientSideDetectionService::kNegativeCacheInterval +
        base::TimeDelta::FromMinutes(5);
    cache[GURL("http://first.url.com/")] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(false,
                                                                   time));

    time = now - ClientSideDetectionService::kNegativeCacheInterval -
        base::TimeDelta::FromHours(1);
    cache[GURL("http://second.url.com/")] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(false,
                                                                   time));

    time = now - ClientSideDetectionService::kPositiveCacheInterval -
        base::TimeDelta::FromMinutes(5);
    cache[GURL("http://third.url.com/")] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(true, time));

    time = now - ClientSideDetectionService::kPositiveCacheInterval +
        base::TimeDelta::FromMinutes(5);
    cache[GURL("http://fourth.url.com/")] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(true, time));

    csd_service_->UpdateCache();

    // 3 elements should be in the cache, the first, third, and fourth.
    EXPECT_EQ(3U, cache.size());
    EXPECT_TRUE(cache.find(GURL("http://first.url.com/")) != cache.end());
    EXPECT_TRUE(cache.find(GURL("http://third.url.com/")) != cache.end());
    EXPECT_TRUE(cache.find(GURL("http://fourth.url.com/")) != cache.end());

    // While 3 elements remain, only the first and the fourth are actually
    // valid.
    bool is_phishing;
    EXPECT_TRUE(csd_service_->GetValidCachedResult(
        GURL("http://first.url.com"), &is_phishing));
    EXPECT_FALSE(is_phishing);
    EXPECT_FALSE(csd_service_->GetValidCachedResult(
        GURL("http://third.url.com"), &is_phishing));
    EXPECT_TRUE(csd_service_->GetValidCachedResult(
        GURL("http://fourth.url.com"), &is_phishing));
    EXPECT_TRUE(is_phishing);
  }

 protected:
  scoped_ptr<ClientSideDetectionService> csd_service_;
  scoped_ptr<FakeURLFetcherFactory> factory_;
  MessageLoop msg_loop_;

 private:
  void GetModelFileDone(base::PlatformFile model_file) {
    model_file_ = model_file;
    msg_loop_.Quit();
  }

  void SendRequestDone(GURL phishing_url, bool is_phishing) {
    ASSERT_EQ(phishing_url, phishing_url_);
    is_phishing_ = is_phishing;
    msg_loop_.Quit();
  }

  scoped_ptr<BrowserThread> browser_thread_;
  base::PlatformFile model_file_;
  scoped_ptr<BrowserThread> file_thread_;

  GURL phishing_url_;
  bool is_phishing_;
};

TEST_F(ClientSideDetectionServiceTest, TestFetchingModel) {
  ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  FilePath model_path = tmp_dir.path().AppendASCII("model");

  // The first time we create the csd service the model file does not exist so
  // we expect there to be a fetch.
  SetModelFetchResponse("BOGUS MODEL", true);
  csd_service_.reset(ClientSideDetectionService::Create(model_path, NULL));
  base::PlatformFile model_file = GetModelFile();
  EXPECT_NE(model_file, base::kInvalidPlatformFileValue);
  EXPECT_EQ(ReadModelFile(model_file), "BOGUS MODEL");

  // If you call GetModelFile() multiple times you always get the same platform
  // file back.  We don't re-open the file.
  EXPECT_EQ(GetModelFile(), model_file);

  // The second time the model already exists on disk.  In this case there
  // should not be any fetch.  To ensure that we clear the factory.
  factory_->ClearFakeReponses();
  csd_service_.reset(ClientSideDetectionService::Create(model_path, NULL));
  model_file = GetModelFile();
  EXPECT_NE(model_file, base::kInvalidPlatformFileValue);
  EXPECT_EQ(ReadModelFile(model_file), "BOGUS MODEL");

  // If the model does not exist and the fetch fails we should get an error.
  model_path = tmp_dir.path().AppendASCII("another_model");
  SetModelFetchResponse("", false /* success */);
  csd_service_.reset(ClientSideDetectionService::Create(model_path, NULL));
  EXPECT_EQ(GetModelFile(), base::kInvalidPlatformFileValue);
}

TEST_F(ClientSideDetectionServiceTest, ServiceObjectDeletedBeforeCallbackDone) {
  SetModelFetchResponse("bogus model", true /* success */);
  ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  csd_service_.reset(ClientSideDetectionService::Create(
      tmp_dir.path().AppendASCII("model"), NULL));
  EXPECT_TRUE(csd_service_.get() != NULL);
  // We delete the client-side detection service class even though the callbacks
  // haven't run yet.
  csd_service_.reset();
  // Waiting for the callbacks to run should not crash even if the service
  // object is gone.
  msg_loop_.RunAllPending();
}

TEST_F(ClientSideDetectionServiceTest, SendClientReportPhishingRequest) {
  SetModelFetchResponse("bogus model", true /* success */);
  ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  csd_service_.reset(ClientSideDetectionService::Create(
      tmp_dir.path().AppendASCII("model"), NULL));

  GURL url("http://a.com/");
  double score = 0.4;  // Some random client score.

  base::Time before = base::Time::Now();

  // Invalid response body from the server.
  SetClientReportPhishingResponse("invalid proto response", true /* success */);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score));

  // Normal behavior.
  ClientPhishingResponse response;
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(),
                                  true /* success */);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score));

  // This request will fail
  GURL second_url("http://b.com/");
  response.set_phishy(false);
  SetClientReportPhishingResponse(response.SerializeAsString(),
                                  false /* success*/);
  EXPECT_FALSE(SendClientReportPhishingRequest(second_url, score));

  base::Time after = base::Time::Now();

  // Check that we have recorded all 3 requests within the correct time range.
  std::queue<base::Time>& report_times = GetPhishingReportTimes();
  EXPECT_EQ(3U, report_times.size());
  while (!report_times.empty()) {
    base::Time time = report_times.back();
    report_times.pop();
    EXPECT_LE(before, time);
    EXPECT_GE(after, time);
  }

  // Only the first url should be in the cache.
  bool is_phishing;
  EXPECT_TRUE(csd_service_->IsInCache(url));
  EXPECT_TRUE(csd_service_->GetValidCachedResult(url, &is_phishing));
  EXPECT_TRUE(is_phishing);
  EXPECT_FALSE(csd_service_->IsInCache(second_url));
}

TEST_F(ClientSideDetectionServiceTest, GetNumReportTest) {
  SetModelFetchResponse("bogus model", true /* success */);
  ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  csd_service_.reset(ClientSideDetectionService::Create(
      tmp_dir.path().AppendASCII("model"), NULL));

  std::queue<base::Time>& report_times = GetPhishingReportTimes();
  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::TimeDelta::FromHours(25);
  report_times.push(now - twenty_five_hours);
  report_times.push(now - twenty_five_hours);
  report_times.push(now);
  report_times.push(now);

  EXPECT_EQ(2, GetNumReports());
}

TEST_F(ClientSideDetectionServiceTest, CacheTest) {
  SetModelFetchResponse("bogus model", true /* success */);
  ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  csd_service_.reset(ClientSideDetectionService::Create(
      tmp_dir.path().AppendASCII("model"), NULL));

  TestCache();
}

TEST_F(ClientSideDetectionServiceTest, IsPrivateIPAddress) {
  SetModelFetchResponse("bogus model", true /* success */);
  ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  csd_service_.reset(ClientSideDetectionService::Create(
      tmp_dir.path().AppendASCII("model"), NULL));

  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("10.1.2.3"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("127.0.0.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("172.24.3.4"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("192.168.1.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("fc00::"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("fec0::"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("fec0:1:2::3"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("::1"));

  EXPECT_FALSE(csd_service_->IsPrivateIPAddress("1.2.3.4"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress("200.1.1.1"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress("2001:0db8:ac10:fe01::"));

  // If the address can't be parsed, the default is true.
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("blah"));
}

}  // namespace safe_browsing
