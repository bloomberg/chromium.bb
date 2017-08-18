// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"

#include <list>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/test/empty_client.h"
#include "components/download/internal/test/test_download_service.h"
#include "components/download/public/download_metadata.h"
#include "components/download/public/service_config.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;
const char kDownloadId[] = "1234Ab";
const char kDownloadId2[] = "Abcd";
const char kFailedDownloadId[] = "f1f1FF";
const char kDownloadLocation[] = "page/1";
const char kDownloadLocation2[] = "page/zz";
const char kServerPathForDownload[] = "/v1/media/page/1";
}  // namespace

namespace offline_pages {

class TestDownloadClient : public download::test::EmptyClient {
 public:
  explicit TestDownloadClient(PrefetchDownloader* downloader)
      : downloader_(downloader) {}

  ~TestDownloadClient() override = default;

  void OnDownloadFailed(const std::string& guid,
                        download::Client::FailureReason reason) override {
    downloader_->OnDownloadFailed(guid);
  }

  void OnDownloadSucceeded(
      const std::string& guid,
      const download::CompletionInfo& completion_info) override {
    downloader_->OnDownloadSucceeded(guid, completion_info.path,
                                     completion_info.bytes_downloaded);
  }

 private:
  PrefetchDownloader* downloader_;
};

class PrefetchDownloaderTest : public testing::Test {
 public:
  PrefetchDownloaderTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_) {}

  void SetUp() override {
    prefetch_service_taco_.reset(new PrefetchServiceTestTaco);
    dispatcher_ = new TestPrefetchDispatcher();

    auto downloader = base::MakeUnique<PrefetchDownloaderImpl>(
        &download_service_, kTestChannel);
    download_service_.SetFailedDownload(kFailedDownloadId, false);
    download_client_ = base::MakeUnique<TestDownloadClient>(downloader.get());
    download_service_.set_client(download_client_.get());
    prefetch_service_taco_->SetPrefetchDispatcher(
        base::WrapUnique(dispatcher_));
    prefetch_service_taco_->SetPrefetchDownloader(std::move(downloader));
    prefetch_service_taco_->CreatePrefetchService();
  }

  void TearDown() override {
    prefetch_service_taco_.reset();
    PumpLoop();
  }

  void SetDownloadServiceReady(bool ready) {
    download_service_.set_is_ready(ready);
    if (ready) {
      GetPrefetchDownloader()->OnDownloadServiceReady(
          std::vector<std::string>());
    } else {
      GetPrefetchDownloader()->OnDownloadServiceShutdown();
    }
  }

  void StartDownload(const std::string& download_id,
                     const std::string& download_location) {
    GetPrefetchDownloader()->StartDownload(download_id, download_location);
  }

  void CancelDownload(const std::string& download_id) {
    GetPrefetchDownloader()->CancelDownload(download_id);
  }

  base::Optional<download::DownloadParams> GetDownload(
      const std::string& guid) const {
    return download_service_.GetDownload(guid);
  }

  void PumpLoop() { task_runner_->RunUntilIdle(); }

  const std::vector<PrefetchDownloadResult>& completed_downloads() const {
    return dispatcher_->download_results;
  }

 private:
  PrefetchDownloader* GetPrefetchDownloader() const {
    return prefetch_service_taco_->prefetch_service()->GetPrefetchDownloader();
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  download::test::TestDownloadService download_service_;
  std::unique_ptr<TestDownloadClient> download_client_;
  std::unique_ptr<PrefetchServiceTestTaco> prefetch_service_taco_;
  TestPrefetchDispatcher* dispatcher_;
};

TEST_F(PrefetchDownloaderTest, DownloadParams) {
  SetDownloadServiceReady(true);
  StartDownload(kDownloadId, kDownloadLocation);
  base::Optional<download::DownloadParams> params = GetDownload(kDownloadId);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(kDownloadId, params->guid);
  EXPECT_EQ(download::DownloadClient::OFFLINE_PAGE_PREFETCH, params->client);
  GURL download_url = params->request_params.url;
  EXPECT_TRUE(download_url.SchemeIs(url::kHttpsScheme));
  EXPECT_EQ(kServerPathForDownload, download_url.path());
  std::string key_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(download_url, "key", &key_value));
  EXPECT_FALSE(key_value.empty());
  std::string alt_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(download_url, "alt", &alt_value));
  EXPECT_EQ("media", alt_value);
  PumpLoop();
}

TEST_F(PrefetchDownloaderTest, StartDownloadBeforeServiceReady) {
  SetDownloadServiceReady(false);
  StartDownload(kDownloadId, kDownloadLocation);
  StartDownload(kDownloadId2, kDownloadLocation2);
  PumpLoop();
  ASSERT_EQ(0u, completed_downloads().size());
  SetDownloadServiceReady(true);
  PumpLoop();
  ASSERT_EQ(2u, completed_downloads().size());
  EXPECT_EQ(kDownloadId, completed_downloads()[0].download_id);
  EXPECT_TRUE(completed_downloads()[0].success);
  EXPECT_EQ(kDownloadId2, completed_downloads()[1].download_id);
  EXPECT_TRUE(completed_downloads()[1].success);
}

TEST_F(PrefetchDownloaderTest, StartDownloadAfterServiceReady) {
  SetDownloadServiceReady(true);
  StartDownload(kDownloadId, kDownloadLocation);
  StartDownload(kDownloadId2, kDownloadLocation2);
  PumpLoop();
  ASSERT_EQ(2u, completed_downloads().size());
  EXPECT_EQ(kDownloadId, completed_downloads()[0].download_id);
  EXPECT_TRUE(completed_downloads()[0].success);
  EXPECT_EQ(kDownloadId2, completed_downloads()[1].download_id);
  EXPECT_TRUE(completed_downloads()[1].success);
}

TEST_F(PrefetchDownloaderTest, DownloadFailed) {
  SetDownloadServiceReady(true);
  StartDownload(kFailedDownloadId, kDownloadLocation);
  PumpLoop();
  ASSERT_EQ(1u, completed_downloads().size());
  EXPECT_EQ(kFailedDownloadId, completed_downloads()[0].download_id);
  EXPECT_FALSE(completed_downloads()[0].success);
}

TEST_F(PrefetchDownloaderTest, CancelPendingDownloadBeforeServiceReady) {
  SetDownloadServiceReady(false);
  StartDownload(kDownloadId, kDownloadLocation);
  StartDownload(kDownloadId2, kDownloadLocation2);
  PumpLoop();
  ASSERT_EQ(0u, completed_downloads().size());
  CancelDownload(kDownloadId);
  SetDownloadServiceReady(true);
  PumpLoop();
  ASSERT_EQ(1u, completed_downloads().size());
  EXPECT_EQ(kDownloadId2, completed_downloads()[0].download_id);
  EXPECT_TRUE(completed_downloads()[0].success);
}

TEST_F(PrefetchDownloaderTest, CancelStartedDownloadBeforeServiceReady) {
  SetDownloadServiceReady(true);
  StartDownload(kDownloadId, kDownloadLocation);
  SetDownloadServiceReady(false);
  CancelDownload(kDownloadId);
  SetDownloadServiceReady(true);
  PumpLoop();
  ASSERT_EQ(0u, completed_downloads().size());
}

TEST_F(PrefetchDownloaderTest, CancelDownloadAfterServiceReady) {
  SetDownloadServiceReady(true);
  StartDownload(kDownloadId, kDownloadLocation);
  StartDownload(kDownloadId2, kDownloadLocation2);
  CancelDownload(kDownloadId);
  PumpLoop();
  ASSERT_EQ(1u, completed_downloads().size());
  EXPECT_EQ(kDownloadId2, completed_downloads()[0].download_id);
  EXPECT_TRUE(completed_downloads()[0].success);
}

}  // namespace offline_pages
