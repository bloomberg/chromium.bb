// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/version.h"
#include "components/component_updater/component_updater_ping_manager.h"
#include "components/component_updater/crx_update_item.h"
#include "components/component_updater/test/test_configurator.h"
#include "components/component_updater/test/url_request_post_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class ComponentUpdaterPingManagerTest : public testing::Test {
 public:
  ComponentUpdaterPingManagerTest();
  virtual ~ComponentUpdaterPingManagerTest() {}

  void RunThreadsUntilIdle();

  // Overrides from testing::Test.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<TestConfigurator> config_;
  scoped_ptr<PingManager> ping_manager_;

 private:
  base::MessageLoopForIO loop_;
};

ComponentUpdaterPingManagerTest::ComponentUpdaterPingManagerTest()
    : config_(new TestConfigurator(base::MessageLoopProxy::current(),
                                   base::MessageLoopProxy::current())) {
}

void ComponentUpdaterPingManagerTest::SetUp() {
  ping_manager_.reset(new PingManager(*config_));
}

void ComponentUpdaterPingManagerTest::TearDown() {
  ping_manager_.reset();
  config_.reset();
}

void ComponentUpdaterPingManagerTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

// Test is flaky: http://crbug.com/349547
TEST_F(ComponentUpdaterPingManagerTest, DISABLED_PingManagerTest) {
  scoped_ptr<InterceptorFactory> interceptor_factory(
      new InterceptorFactory(base::MessageLoopProxy::current()));
  URLRequestPostInterceptor* interceptor =
      interceptor_factory->CreateInterceptor();
  EXPECT_TRUE(interceptor);

  // Test eventresult="1" is sent for successful updates.
  CrxUpdateItem item;
  item.id = "abc";
  item.status = CrxUpdateItem::kUpdated;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");

  ping_manager_->OnUpdateComplete(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(string::npos,
            interceptor->GetRequests()[0].find(
                "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
                "<event eventtype=\"3\" eventresult=\"1\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();

  // Test eventresult="0" is sent for failed updates.
  item = CrxUpdateItem();
  item.id = "abc";
  item.status = CrxUpdateItem::kNoUpdate;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");

  ping_manager_->OnUpdateComplete(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(string::npos,
            interceptor->GetRequests()[0].find(
                "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
                "<event eventtype=\"3\" eventresult=\"0\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();

  // Test the error values and the fingerprints.
  item = CrxUpdateItem();
  item.id = "abc";
  item.status = CrxUpdateItem::kNoUpdate;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");
  item.previous_fp = "prev fp";
  item.next_fp = "next fp";
  item.error_category = 1;
  item.error_code = 2;
  item.extra_code1 = -1;
  item.diff_error_category = 10;
  item.diff_error_code = 20;
  item.diff_extra_code1 = -10;
  item.diff_update_failed = true;
  item.crx_diffurls.push_back(GURL("http://host/path"));

  ping_manager_->OnUpdateComplete(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(string::npos,
            interceptor->GetRequests()[0].find(
                "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
                "<event eventtype=\"3\" eventresult=\"0\" errorcat=\"1\" "
                "errorcode=\"2\" extracode1=\"-1\" diffresult=\"0\" "
                "differrorcat=\"10\" "
                "differrorcode=\"20\" diffextracode1=\"-10\" "
                "previousfp=\"prev fp\" nextfp=\"next fp\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();

  // Test the download metrics.
  item = CrxUpdateItem();
  item.id = "abc";
  item.status = CrxUpdateItem::kUpdated;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");

  CrxDownloader::DownloadMetrics download_metrics;
  download_metrics.url = GURL("http://host1/path1");
  download_metrics.downloader = CrxDownloader::DownloadMetrics::kUrlFetcher;
  download_metrics.error = -1;
  download_metrics.downloaded_bytes = 123;
  download_metrics.total_bytes = 456;
  download_metrics.download_time_ms = 987;
  item.download_metrics.push_back(download_metrics);

  download_metrics = CrxDownloader::DownloadMetrics();
  download_metrics.url = GURL("http://host2/path2");
  download_metrics.downloader = CrxDownloader::DownloadMetrics::kBits;
  download_metrics.error = 0;
  download_metrics.downloaded_bytes = 1230;
  download_metrics.total_bytes = 4560;
  download_metrics.download_time_ms = 9870;
  item.download_metrics.push_back(download_metrics);

  ping_manager_->OnUpdateComplete(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(
      string::npos,
      interceptor->GetRequests()[0].find(
          "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
          "<event eventtype=\"3\" eventresult=\"1\"/>"
          "<event eventtype=\"14\" eventresult=\"0\" downloader=\"direct\" "
          "errorcode=\"-1\" url=\"http://host1/path1\" downloaded=\"123\" "
          "total=\"456\" download_time_ms=\"987\"/>"
          "<event eventtype=\"14\" eventresult=\"1\" downloader=\"bits\" "
          "url=\"http://host2/path2\" downloaded=\"1230\" total=\"4560\" "
          "download_time_ms=\"9870\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();
}

}  // namespace component_updater
