// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;
using content::BrowserThread;

namespace safe_browsing {
namespace {
class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService() : ClientSideDetectionService(NULL) {}
  virtual ~MockClientSideDetectionService() {}

  MOCK_METHOD1(EndFetchModel, void(ClientModelStatus));
  MOCK_METHOD1(ScheduleFetchModel, void(int64));

  void Schedule(int64) {
    // Ignore the delay when testing.
    StartFetchModel();
  }

  void Disable(int) {
    // Ignore the status.
    SetEnabledAndRefreshState(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSideDetectionService);
};

ACTION(QuitCurrentMessageLoop) {
  MessageLoop::current()->Quit();
}

}  // namespace

class ClientSideDetectionServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    file_thread_.reset(new content::TestBrowserThread(BrowserThread::FILE,
                                                      &msg_loop_));

    factory_.reset(new net::FakeURLFetcherFactory(NULL));

    browser_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                         &msg_loop_));
  }

  virtual void TearDown() {
    msg_loop_.RunUntilIdle();
    csd_service_.reset();
    file_thread_.reset();
    browser_thread_.reset();
  }

  bool SendClientReportPhishingRequest(const GURL& phishing_url,
                                       float score) {
    ClientPhishingRequest* request = new ClientPhishingRequest();
    request->set_url(phishing_url.spec());
    request->set_client_score(score);
    request->set_is_phishing(true);  // client thinks the URL is phishing.
    csd_service_->SendClientReportPhishingRequest(
        request,
        base::Bind(&ClientSideDetectionServiceTest::SendRequestDone,
                   base::Unretained(this)));
    phishing_url_ = phishing_url;
    msg_loop_.Run();  // Waits until callback is called.
    return is_phishing_;
  }

  bool SendClientReportMalwareRequest(const GURL& url) {
    scoped_ptr<ClientMalwareRequest> request(new ClientMalwareRequest());
    request->set_url(url.spec());
    csd_service_->SendClientReportMalwareRequest(
        request.release(),
        base::Bind(&ClientSideDetectionServiceTest::SendMalwareRequestDone,
                   base::Unretained(this)));
    phishing_url_ = url;
    msg_loop_.Run();  // Waits until callback is called.
    return is_malware_;
  }

  void SetModelFetchResponse(std::string response_data, bool success) {
    factory_->SetFakeResponse(ClientSideDetectionService::kClientModelUrl,
                              response_data, success);
  }

  void SetClientReportPhishingResponse(std::string response_data,
                                       bool success) {
    factory_->SetFakeResponse(
        ClientSideDetectionService::GetClientReportUrl(
            ClientSideDetectionService::kClientReportPhishingUrl),
        response_data, success);
  }

  void SetClientReportMalwareResponse(std::string response_data,
                                      bool success) {
    factory_->SetFakeResponse(
        ClientSideDetectionService::GetClientReportUrl(
            ClientSideDetectionService::kClientReportMalwareUrl),
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
    base::Time time =
        now - base::TimeDelta::FromDays(
            ClientSideDetectionService::kNegativeCacheIntervalDays) +
        base::TimeDelta::FromMinutes(5);
    cache[GURL("http://first.url.com/")] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(false,
                                                                   time));

    time =
        now - base::TimeDelta::FromDays(
            ClientSideDetectionService::kNegativeCacheIntervalDays) -
        base::TimeDelta::FromHours(1);
    cache[GURL("http://second.url.com/")] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(false,
                                                                   time));

    time =
        now - base::TimeDelta::FromMinutes(
            ClientSideDetectionService::kPositiveCacheIntervalMinutes) -
        base::TimeDelta::FromMinutes(5);
    cache[GURL("http://third.url.com/")] =
        make_linked_ptr(new ClientSideDetectionService::CacheState(true, time));

    time =
        now - base::TimeDelta::FromMinutes(
            ClientSideDetectionService::kPositiveCacheIntervalMinutes) +
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

  void AddFeature(const std::string& name, double value,
                  ClientPhishingRequest* request) {
    ClientPhishingRequest_Feature* feature = request->add_feature_map();
    feature->set_name(name);
    feature->set_value(value);
  }

  void AddNonModelFeature(const std::string& name, double value,
                          ClientPhishingRequest* request) {
    ClientPhishingRequest_Feature* feature =
        request->add_non_model_feature_map();
    feature->set_name(name);
    feature->set_value(value);
  }

 protected:
  scoped_ptr<ClientSideDetectionService> csd_service_;
  scoped_ptr<net::FakeURLFetcherFactory> factory_;
  MessageLoop msg_loop_;

 private:
  void SendRequestDone(GURL phishing_url, bool is_phishing) {
    ASSERT_EQ(phishing_url, phishing_url_);
    is_phishing_ = is_phishing;
    msg_loop_.Quit();
  }

  void SendMalwareRequestDone(GURL url, bool is_malware) {
    ASSERT_EQ(phishing_url_, url);
    is_malware_ = is_malware;
    msg_loop_.Quit();
  }
  scoped_ptr<content::TestBrowserThread> browser_thread_;
  scoped_ptr<content::TestBrowserThread> file_thread_;

  GURL phishing_url_;
  bool is_phishing_;
  bool is_malware_;
};

TEST_F(ClientSideDetectionServiceTest, FetchModelTest) {
  // We don't want to use a real service class here because we can't call
  // the real EndFetchModel.  It would reschedule a reload which might
  // make the test flaky.
  MockClientSideDetectionService service;
  EXPECT_CALL(service, ScheduleFetchModel(_)).Times(1);
  service.SetEnabledAndRefreshState(true);

  // The model fetch failed.
  SetModelFetchResponse("blamodel", false /* failure */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_FETCH_FAILED))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Empty model file.
  SetModelFetchResponse("", true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_EMPTY))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Model is too large.
  SetModelFetchResponse(
      std::string(ClientSideDetectionService::kMaxModelSizeBytes + 1, 'x'),
      true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_TOO_LARGE))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Unable to parse the model file.
  SetModelFetchResponse("Invalid model file", true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_PARSE_ERROR))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Model that is missing some required fields (missing the version field).
  ClientSideModel model;
  model.set_max_words_per_term(4);
  SetModelFetchResponse(model.SerializePartialAsString(), true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_MISSING_FIELDS))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Model that points to hashes that don't exist.
  model.set_version(10);
  model.add_hashes("bla");
  model.add_page_term(1);  // Should be 0 instead of 1.
  SetModelFetchResponse(model.SerializePartialAsString(), true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_BAD_HASH_IDS))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);
  model.set_page_term(0, 0);

  // Model version number is wrong.
  model.set_version(-1);
  SetModelFetchResponse(model.SerializeAsString(), true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_INVALID_VERSION_NUMBER))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Normal model.
  model.set_version(10);
  SetModelFetchResponse(model.SerializeAsString(), true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_SUCCESS))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Model version number is decreasing.  Set the model version number of the
  // model that is currently loaded in the service object to 11.
  service.model_.reset(new ClientSideModel(model));
  service.model_->set_version(11);
  SetModelFetchResponse(model.SerializeAsString(), true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_INVALID_VERSION_NUMBER))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);

  // Model version hasn't changed since the last reload.
  service.model_->set_version(10);
  SetModelFetchResponse(model.SerializeAsString(), true /* success */);
  EXPECT_CALL(service, EndFetchModel(
      ClientSideDetectionService::MODEL_NOT_CHANGED))
      .WillOnce(QuitCurrentMessageLoop());
  service.StartFetchModel();
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(&service);
}

TEST_F(ClientSideDetectionServiceTest, ServiceObjectDeletedBeforeCallbackDone) {
  SetModelFetchResponse("bogus model", true /* success */);
  csd_service_.reset(ClientSideDetectionService::Create(NULL));
  csd_service_->SetEnabledAndRefreshState(true);
  EXPECT_TRUE(csd_service_.get() != NULL);
  // We delete the client-side detection service class even though the callbacks
  // haven't run yet.
  csd_service_.reset();
  // Waiting for the callbacks to run should not crash even if the service
  // object is gone.
  msg_loop_.RunUntilIdle();
}

TEST_F(ClientSideDetectionServiceTest, SendClientReportPhishingRequest) {
  SetModelFetchResponse("bogus model", true /* success */);
  csd_service_.reset(ClientSideDetectionService::Create(NULL));
  csd_service_->SetEnabledAndRefreshState(true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.

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
                                  false /* success */);
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

TEST_F(ClientSideDetectionServiceTest, SendClientReportMalwareRequest) {
  SetModelFetchResponse("bogus model", true /* success */);
  csd_service_.reset(ClientSideDetectionService::Create(NULL));
  csd_service_->SetEnabledAndRefreshState(true);
  GURL url("http://a.com/");

  // Invalid response body from the server.
  SetClientReportMalwareResponse("invalid proto response", true /* success */);
  EXPECT_FALSE(SendClientReportMalwareRequest(url));

  // Normal behavior.
  ClientMalwareResponse response;
  response.set_blacklist(true);
  SetClientReportMalwareResponse(response.SerializeAsString(), true);
  EXPECT_TRUE(SendClientReportMalwareRequest(url));

  // This request will fail
  response.set_blacklist(false);
  SetClientReportMalwareResponse(response.SerializeAsString(),
                                 false /* success */);
  EXPECT_FALSE(SendClientReportMalwareRequest(url));

  // server blacklist decision is false, and response is succesful
  response.set_blacklist(false);
  SetClientReportMalwareResponse(response.SerializeAsString(), true);
  EXPECT_FALSE(SendClientReportMalwareRequest(url));
}

TEST_F(ClientSideDetectionServiceTest, GetNumReportTest) {
  SetModelFetchResponse("bogus model", true /* success */);
  csd_service_.reset(ClientSideDetectionService::Create(NULL));

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
  csd_service_.reset(ClientSideDetectionService::Create(NULL));

  TestCache();
}

TEST_F(ClientSideDetectionServiceTest, IsPrivateIPAddress) {
  SetModelFetchResponse("bogus model", true /* success */);
  csd_service_.reset(ClientSideDetectionService::Create(NULL));

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

TEST_F(ClientSideDetectionServiceTest, SetBadSubnets) {
  ClientSideModel model;
  ClientSideDetectionService::BadSubnetMap bad_subnets;
  ClientSideDetectionService::SetBadSubnets(model, &bad_subnets);
  EXPECT_EQ(0U, bad_subnets.size());

  // Bad subnets are skipped.
  ClientSideModel::IPSubnet* subnet = model.add_bad_subnet();
  subnet->set_prefix(std::string(crypto::kSHA256Length, '.'));
  subnet->set_size(130);  // Invalid size.

  subnet = model.add_bad_subnet();
  subnet->set_prefix(std::string(crypto::kSHA256Length, '.'));
  subnet->set_size(-1);  // Invalid size.

  subnet = model.add_bad_subnet();
  subnet->set_prefix(std::string(16, '.'));  // Invalid len.
  subnet->set_size(64);

  ClientSideDetectionService::SetBadSubnets(model, &bad_subnets);
  EXPECT_EQ(0U, bad_subnets.size());

  subnet = model.add_bad_subnet();
  subnet->set_prefix(std::string(crypto::kSHA256Length, '.'));
  subnet->set_size(64);

  subnet = model.add_bad_subnet();
  subnet->set_prefix(std::string(crypto::kSHA256Length, ','));
  subnet->set_size(64);

  subnet = model.add_bad_subnet();
  subnet->set_prefix(std::string(crypto::kSHA256Length, '.'));
  subnet->set_size(128);

  subnet = model.add_bad_subnet();
  subnet->set_prefix(std::string(crypto::kSHA256Length, '.'));
  subnet->set_size(100);

  ClientSideDetectionService::SetBadSubnets(model, &bad_subnets);
  EXPECT_EQ(3U, bad_subnets.size());
  ClientSideDetectionService::BadSubnetMap::const_iterator it;
  std::string mask = std::string(8, '\xFF') + std::string(8, '\x00');
  EXPECT_TRUE(bad_subnets.count(mask));
  EXPECT_TRUE(bad_subnets[mask].count(std::string(crypto::kSHA256Length, '.')));
  EXPECT_TRUE(bad_subnets[mask].count(std::string(crypto::kSHA256Length, ',')));

  mask = std::string(16, '\xFF');
  EXPECT_TRUE(bad_subnets.count(mask));
  EXPECT_TRUE(bad_subnets[mask].count(std::string(crypto::kSHA256Length, '.')));

  mask = std::string(12, '\xFF') + "\xF0" + std::string(3, '\x00');
  EXPECT_TRUE(bad_subnets.count(mask));
  EXPECT_TRUE(bad_subnets[mask].count(std::string(crypto::kSHA256Length, '.')));
}

TEST_F(ClientSideDetectionServiceTest, IsBadIpAddress) {
  ClientSideModel model;
  // IPv6 exact match for: 2620:0:1000:3103:21a:a0ff:fe10:786e.
  ClientSideModel::IPSubnet* subnet = model.add_bad_subnet();
  subnet->set_prefix(crypto::SHA256HashString(std::string(
      "\x26\x20\x00\x00\x10\x00\x31\x03\x02\x1a\xa0\xff\xfe\x10\x78\x6e", 16)));
  subnet->set_size(128);

  // IPv6 prefix match for: fe80::21a:a0ff:fe10:786e/64.
  subnet = model.add_bad_subnet();
  subnet->set_prefix(crypto::SHA256HashString(std::string(
      "\xfe\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16)));
  subnet->set_size(64);

  // IPv4 exact match for ::ffff:192.0.2.128.
  subnet = model.add_bad_subnet();
  subnet->set_prefix(crypto::SHA256HashString(std::string(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xc0\x00\x02\x80", 16)));
  subnet->set_size(128);

  // IPv4 prefix match (/8) for ::ffff:192.1.1.0.
  subnet = model.add_bad_subnet();
  subnet->set_prefix(crypto::SHA256HashString(std::string(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xc0\x01\x01\x00", 16)));
  subnet->set_size(120);

  // IPv4 prefix match (/9) for ::ffff:192.1.122.0.
  subnet = model.add_bad_subnet();
  subnet->set_prefix(crypto::SHA256HashString(std::string(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xc0\x01\x7a\x00", 16)));
  subnet->set_size(119);

  // IPv4 prefix match (/15) for ::ffff:192.1.128.0.
  subnet = model.add_bad_subnet();
  subnet->set_prefix(crypto::SHA256HashString(std::string(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xc0\x01\x80\x00", 16)));
  subnet->set_size(113);

  csd_service_.reset(ClientSideDetectionService::Create(NULL));
  ClientSideDetectionService::SetBadSubnets(
      model, &(csd_service_->bad_subnets_));
  EXPECT_FALSE(csd_service_->IsBadIpAddress("blabla"));
  EXPECT_FALSE(csd_service_->IsBadIpAddress(""));

  EXPECT_TRUE(csd_service_->IsBadIpAddress(
      "2620:0:1000:3103:21a:a0ff:fe10:786e"));
  EXPECT_FALSE(csd_service_->IsBadIpAddress(
      "2620:0:1000:3103:21a:a0ff:fe10:786f"));

  EXPECT_TRUE(csd_service_->IsBadIpAddress("fe80::21a:a0ff:fe10:786e"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("fe80::31a:a0ff:fe10:786e"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("fe80::21a:a0ff:fe10:786f"));
  EXPECT_FALSE(csd_service_->IsBadIpAddress("fe81::21a:a0ff:fe10:786e"));

  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.0.2.128"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("::ffff:192.0.2.128"));
  EXPECT_FALSE(csd_service_->IsBadIpAddress("192.0.2.129"));

  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.1.0"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.1.255"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.1.10"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("::ffff:192.1.1.2"));

  EXPECT_FALSE(csd_service_->IsBadIpAddress("192.1.121.255"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.122.0"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("::ffff:192.1.122.1"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.122.255"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.123.0"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.123.255"));
  EXPECT_FALSE(csd_service_->IsBadIpAddress("192.1.124.0"));

  EXPECT_FALSE(csd_service_->IsBadIpAddress("192.1.127.255"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.128.0"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("::ffff:192.1.128.1"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.128.255"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.255.0"));
  EXPECT_TRUE(csd_service_->IsBadIpAddress("192.1.255.255"));
  EXPECT_FALSE(csd_service_->IsBadIpAddress("192.2.0.0"));
}

TEST_F(ClientSideDetectionServiceTest, ModelHasValidHashIds) {
  ClientSideModel model;
  EXPECT_TRUE(ClientSideDetectionService::ModelHasValidHashIds(model));
  model.add_hashes("bla");
  EXPECT_TRUE(ClientSideDetectionService::ModelHasValidHashIds(model));
  model.add_page_term(0);
  EXPECT_TRUE(ClientSideDetectionService::ModelHasValidHashIds(model));

  model.add_page_term(-1);
  EXPECT_FALSE(ClientSideDetectionService::ModelHasValidHashIds(model));
  model.set_page_term(1, 1);
  EXPECT_FALSE(ClientSideDetectionService::ModelHasValidHashIds(model));
  model.set_page_term(1, 0);
  EXPECT_TRUE(ClientSideDetectionService::ModelHasValidHashIds(model));

  // Test bad rules.
  model.add_hashes("blu");
  ClientSideModel::Rule* rule = model.add_rule();
  rule->add_feature(0);
  rule->add_feature(1);
  rule->set_weight(0.1f);
  EXPECT_TRUE(ClientSideDetectionService::ModelHasValidHashIds(model));

  rule = model.add_rule();
  rule->add_feature(0);
  rule->add_feature(1);
  rule->add_feature(-1);
  rule->set_weight(0.2f);
  EXPECT_FALSE(ClientSideDetectionService::ModelHasValidHashIds(model));

  rule->set_feature(2, 2);
  EXPECT_FALSE(ClientSideDetectionService::ModelHasValidHashIds(model));

  rule->set_feature(2, 1);
  EXPECT_TRUE(ClientSideDetectionService::ModelHasValidHashIds(model));
}

TEST_F(ClientSideDetectionServiceTest, SetEnabledAndRefreshState) {
  // Check that the model isn't downloaded until the service is enabled.
  csd_service_.reset(ClientSideDetectionService::Create(NULL));
  EXPECT_FALSE(csd_service_->enabled());
  EXPECT_TRUE(csd_service_->model_fetcher_.get() == NULL);

  // Use a MockClientSideDetectionService for the rest of the test, to avoid
  // the scheduling delay.
  MockClientSideDetectionService* service =
      new StrictMock<MockClientSideDetectionService>();
  csd_service_.reset(service);
  EXPECT_FALSE(csd_service_->enabled());
  EXPECT_TRUE(csd_service_->model_fetcher_.get() == NULL);
  // No calls expected yet.
  Mock::VerifyAndClearExpectations(service);

  ClientSideModel model;
  model.set_version(10);
  model.set_max_words_per_term(4);
  SetModelFetchResponse(model.SerializeAsString(), true /* success */);
  EXPECT_CALL(*service, ScheduleFetchModel(_))
      .WillOnce(Invoke(service, &MockClientSideDetectionService::Schedule));
  EXPECT_CALL(*service, EndFetchModel(
      ClientSideDetectionService::MODEL_SUCCESS))
      .WillOnce(QuitCurrentMessageLoop());
  csd_service_->SetEnabledAndRefreshState(true);
  EXPECT_TRUE(csd_service_->model_fetcher_.get() != NULL);
  msg_loop_.Run();  // EndFetchModel will quit the message loop.
  Mock::VerifyAndClearExpectations(service);

  // Check that enabling again doesn't request the model.
  csd_service_->SetEnabledAndRefreshState(true);
  // No calls expected.
  Mock::VerifyAndClearExpectations(service);

  // Check that disabling the service cancels pending requests.
  EXPECT_CALL(*service, ScheduleFetchModel(_))
      .WillOnce(Invoke(service, &MockClientSideDetectionService::Schedule));
  csd_service_->SetEnabledAndRefreshState(false);
  csd_service_->SetEnabledAndRefreshState(true);
  Mock::VerifyAndClearExpectations(service);
  EXPECT_TRUE(csd_service_->model_fetcher_.get() != NULL);
  csd_service_->SetEnabledAndRefreshState(false);
  EXPECT_TRUE(csd_service_->model_fetcher_.get() == NULL);
  msg_loop_.RunUntilIdle();
  // No calls expected.
  Mock::VerifyAndClearExpectations(service);

  // Requests always return false when the service is disabled.
  ClientPhishingResponse response;
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(),
                                  true /* success */);
  EXPECT_FALSE(SendClientReportPhishingRequest(GURL("http://a.com/"), 0.4f));

  // Pending requests also return false if the service is disabled before they
  // report back.
  EXPECT_CALL(*service, ScheduleFetchModel(_))
      .WillOnce(Invoke(service, &MockClientSideDetectionService::Schedule));
  EXPECT_CALL(*service, EndFetchModel(
      ClientSideDetectionService::MODEL_NOT_CHANGED))
      .WillOnce(Invoke(service, &MockClientSideDetectionService::Disable));
  csd_service_->SetEnabledAndRefreshState(true);
  EXPECT_FALSE(SendClientReportPhishingRequest(GURL("http://a.com/"), 0.4f));
  Mock::VerifyAndClearExpectations(service);
}
}  // namespace safe_browsing
