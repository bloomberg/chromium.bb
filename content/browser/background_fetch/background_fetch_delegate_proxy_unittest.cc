// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"

#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/background_fetch_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FakeBackgroundFetchDelegate : public BackgroundFetchDelegate {
 public:
  FakeBackgroundFetchDelegate() {}

  // BackgroundFetchDelegate implementation:
  void CreateDownloadJob(
      const std::string& job_unique_id,
      const std::string& title,
      const url::Origin& origin,
      int completed_parts,
      int total_parts,
      const std::vector<std::string>& current_guids) override {}
  void DownloadUrl(const std::string& job_unique_id,
                   const std::string& guid,
                   const std::string& method,
                   const GURL& url,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers) override {
    if (!client())
      return;

    download_guid_to_job_id_map_[guid] = job_unique_id;

    auto response = std::make_unique<BackgroundFetchResponse>(
        std::vector<GURL>({url}),
        base::MakeRefCounted<net::HttpResponseHeaders>("200 OK"));

    client()->OnDownloadStarted(guid, std::move(response));
    if (complete_downloads_) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::BindOnce(&FakeBackgroundFetchDelegate::CompleteDownload,
                         base::Unretained(this), guid));
    }
  }
  void Abort(const std::string& job_unique_id) override {
    aborted_jobs_.insert(job_unique_id);
  }

  void set_complete_downloads(bool complete_downloads) {
    complete_downloads_ = complete_downloads;
  }

 private:
  void CompleteDownload(const std::string& guid) {
    if (!client())
      return;

    if (aborted_jobs_.count(download_guid_to_job_id_map_[guid]))
      return;

    client()->OnDownloadComplete(
        guid, std::make_unique<BackgroundFetchResult>(base::Time::Now(),
                                                      base::FilePath(), 10u));
  }

  std::set<std::string> aborted_jobs_;
  std::map<std::string, std::string> download_guid_to_job_id_map_;
  bool complete_downloads_ = true;
};

class FakeController : public BackgroundFetchDelegateProxy::Controller {
 public:
  FakeController() : weak_ptr_factory_(this) {}

  void DidStartRequest(const scoped_refptr<BackgroundFetchRequestInfo>& request,
                       const std::string& download_guid) override {
    request_started_ = true;
  }

  void DidUpdateRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request,
      const std::string& download_guid,
      uint64_t bytes_downloaded) override {}

  void DidCompleteRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request,
      const std::string& download_guid) override {
    request_completed_ = true;
  }

  bool request_started_ = false;
  bool request_completed_ = false;
  base::WeakPtrFactory<FakeController> weak_ptr_factory_;
};

class BackgroundFetchDelegateProxyTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchDelegateProxyTest() : delegate_proxy_(&delegate_) {}

 protected:
  FakeBackgroundFetchDelegate delegate_;
  BackgroundFetchDelegateProxy delegate_proxy_;
};

}  // namespace

TEST_F(BackgroundFetchDelegateProxyTest, SetDelegate) {
  EXPECT_TRUE(delegate_.client().get());
}

TEST_F(BackgroundFetchDelegateProxyTest, StartRequest) {
  FakeController controller;
  ServiceWorkerFetchRequest fetch_request;
  auto request = base::MakeRefCounted<BackgroundFetchRequestInfo>(
      0 /* request_index */, fetch_request);

  EXPECT_FALSE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);

  delegate_proxy_.StartRequest("jobid1",
                               controller.weak_ptr_factory_.GetWeakPtr(),
                               url::Origin(), request);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.request_started_);
  EXPECT_TRUE(controller.request_completed_);
}

TEST_F(BackgroundFetchDelegateProxyTest, StartRequest_NotCompleted) {
  FakeController controller;
  ServiceWorkerFetchRequest fetch_request;
  auto request = base::MakeRefCounted<BackgroundFetchRequestInfo>(
      0 /* request_index */, fetch_request);

  EXPECT_FALSE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);

  delegate_.set_complete_downloads(false);
  delegate_proxy_.StartRequest("jobid1",
                               controller.weak_ptr_factory_.GetWeakPtr(),
                               url::Origin(), request);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);
}

TEST_F(BackgroundFetchDelegateProxyTest, Abort) {
  FakeController controller;
  FakeController controller2;
  ServiceWorkerFetchRequest fetch_request;
  ServiceWorkerFetchRequest fetch_request2;
  auto request = base::MakeRefCounted<BackgroundFetchRequestInfo>(
      0 /* request_index */, fetch_request);
  auto request2 = base::MakeRefCounted<BackgroundFetchRequestInfo>(
      1 /* request_index */, fetch_request2);

  EXPECT_FALSE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_);
  EXPECT_FALSE(controller2.request_started_);
  EXPECT_FALSE(controller2.request_completed_);

  delegate_proxy_.StartRequest("jobid1",
                               controller.weak_ptr_factory_.GetWeakPtr(),
                               url::Origin(), request);
  delegate_proxy_.StartRequest("jobid2",
                               controller2.weak_ptr_factory_.GetWeakPtr(),
                               url::Origin(), request2);
  delegate_proxy_.Abort("jobid1");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.request_started_);
  EXPECT_FALSE(controller.request_completed_) << "Aborted job completed";
  EXPECT_TRUE(controller2.request_started_);
  EXPECT_TRUE(controller2.request_completed_) << "Normal job did not complete";
}

}  // namespace content
