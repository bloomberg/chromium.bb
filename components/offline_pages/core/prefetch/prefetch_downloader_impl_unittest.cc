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
#include "components/download/public/download_service.h"
#include "components/download/public/service_config.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;
const char kDownloadId[] = "1234";
const char kDownloadId2[] = "ABCD";
const char kFailedDownloadId[] = "FFFFFF";
const char kDownloadLocation[] = "page/1";
const char kDownloadLocation2[] = "page/zz";
const char kServerPathForDownload[] = "/v1/media/page/1";
const uint64_t kTestFileSize = 12345678u;
}  // namespace

namespace download {
class TestServiceConfig : public ServiceConfig {
 public:
  TestServiceConfig() = default;
  ~TestServiceConfig() override = default;

  uint32_t GetMaxScheduledDownloadsPerClient() const override { return 0; }
  const base::TimeDelta& GetFileKeepAliveTime() const override {
    return time_delta_;
  }

 private:
  base::TimeDelta time_delta_;
};

class TestDownloadService : public DownloadService {
 public:
  TestDownloadService() = default;
  ~TestDownloadService() override = default;

  // DownloadService implementation.
  const ServiceConfig& GetConfig() override { return service_config_; }
  void OnStartScheduledTask(DownloadTaskType task_type,
                            const TaskFinishedCallback& callback) override {}
  bool OnStopScheduledTask(DownloadTaskType task_type) override { return true; }
  ServiceStatus GetStatus() override {
    return ready_ ? DownloadService::ServiceStatus::READY
                  : DownloadService::ServiceStatus::STARTING_UP;
  }

  void StartDownload(const DownloadParams& download_params) override {
    if (!ready_) {
      OnDownloadFailed(download_params.guid);
      return;
    }
    downloads_.push_back(download_params);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&TestDownloadService::ProcessDownload,
                              base::Unretained(this)));
  }

  void PauseDownload(const std::string& guid) override {}
  void ResumeDownload(const std::string& guid) override {}

  void CancelDownload(const std::string& guid) override {
    for (auto iter = downloads_.begin(); iter != downloads_.end(); ++iter) {
      if (iter->guid == guid) {
        downloads_.erase(iter);
        return;
      }
    }
  }

  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override {}

  DownloadParams GetDownload(const std::string& guid) const {
    for (auto iter = downloads_.begin(); iter != downloads_.end(); ++iter) {
      if (iter->guid == guid)
        return *iter;
    }
    DownloadParams params;
    params.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    return params;
  }

  void set_ready(bool ready) { ready_ = ready; }
  void set_prefetch_downloader(
      offline_pages::PrefetchDownloaderImpl* prefetch_downloader) {
    prefetch_downloader_ = prefetch_downloader;
  }

 private:
  void ProcessDownload() {
    if (!ready_ || downloads_.empty())
      return;
    DownloadParams params = downloads_.front();
    downloads_.pop_front();
    if (params.guid == kFailedDownloadId)
      OnDownloadFailed(params.guid);
    else
      OnDownloadSucceeded(params.guid, base::FilePath(), kTestFileSize);
  }

  void OnDownloadSucceeded(const std::string& guid,
                           const base::FilePath& file_path,
                           uint64_t file_size) {
    if (prefetch_downloader_)
      prefetch_downloader_->OnDownloadSucceeded(guid, file_path, file_size);
  }

  void OnDownloadFailed(const std::string& guid) {
    if (prefetch_downloader_)
      prefetch_downloader_->OnDownloadFailed(guid);
  }

  bool ready_ = false;
  offline_pages::PrefetchDownloaderImpl* prefetch_downloader_ = nullptr;
  TestServiceConfig service_config_;
  std::list<DownloadParams> downloads_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadService);
};
}  // namespace download

namespace offline_pages {

class PrefetchDownloaderTest : public testing::Test {
 public:
  PrefetchDownloaderTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_) {}

  void SetUp() override {
    prefetch_service_taco_.reset(new PrefetchServiceTestTaco);

    auto downloader = base::MakeUnique<PrefetchDownloaderImpl>(
        &download_service_, kTestChannel);
    download_service_.set_prefetch_downloader(downloader.get());
    prefetch_service_taco_->SetPrefetchDownloader(std::move(downloader));

    prefetch_service_taco_->CreatePrefetchService();
    GetPrefetchDownloader()->SetCompletedCallback(base::Bind(
        &PrefetchDownloaderTest::OnDownloadCompleted, base::Unretained(this)));
  }

  void TearDown() override {
    prefetch_service_taco_.reset();
    PumpLoop();
  }

  void SetDownloadServiceReady(bool ready) {
    download_service_.set_ready(ready);
    if (ready)
      GetPrefetchDownloader()->OnDownloadServiceReady();
    else
      GetPrefetchDownloader()->OnDownloadServiceShutdown();
  }

  void StartDownload(const std::string& download_id,
                     const std::string& download_location) {
    GetPrefetchDownloader()->StartDownload(download_id, download_location);
  }

  void CancelDownload(const std::string& download_id) {
    GetPrefetchDownloader()->CancelDownload(download_id);
  }

  download::DownloadParams GetDownload(const std::string& guid) const {
    return download_service_.GetDownload(guid);
  }

  void PumpLoop() { task_runner_->RunUntilIdle(); }

  const std::vector<PrefetchDownloadResult>& completed_downloads() const {
    return completed_downloads_;
  }

 private:
  void OnDownloadCompleted(const PrefetchDownloadResult& result) {
    completed_downloads_.push_back(result);
  }

  PrefetchDownloader* GetPrefetchDownloader() const {
    return prefetch_service_taco_->prefetch_service()->GetPrefetchDownloader();
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  download::TestDownloadService download_service_;
  std::unique_ptr<PrefetchServiceTestTaco> prefetch_service_taco_;
  std::vector<PrefetchDownloadResult> completed_downloads_;
};

TEST_F(PrefetchDownloaderTest, DownloadParams) {
  SetDownloadServiceReady(true);
  StartDownload(kDownloadId, kDownloadLocation);
  download::DownloadParams params = GetDownload(kDownloadId);
  EXPECT_EQ(kDownloadId, params.guid);
  EXPECT_EQ(download::DownloadClient::OFFLINE_PAGE_PREFETCH, params.client);
  GURL download_url = params.request_params.url;
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
