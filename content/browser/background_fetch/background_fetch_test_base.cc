// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_base.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kTestOrigin[] = "https://example.com/";
const char kTestScriptUrl[] = "https://example.com/sw.js";

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::Closure quit_closure,
                              ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t service_worker_registration_id) {
  DCHECK(out_service_worker_registration_id);
  EXPECT_EQ(SERVICE_WORKER_OK, status) << status_message;

  *out_service_worker_registration_id = service_worker_registration_id;

  quit_closure.Run();
}

void DidFindServiceWorkerRegistration(
    scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
    base::Closure quit_closure,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  quit_closure.Run();
}

}  // namespace

// -----------------------------------------------------------------------------
// TestResponse

BackgroundFetchTestBase::TestResponse::TestResponse() = default;

BackgroundFetchTestBase::TestResponse::~TestResponse() = default;

// -----------------------------------------------------------------------------
// TestResponseBuilder

BackgroundFetchTestBase::TestResponseBuilder::TestResponseBuilder(
    int response_code)
    : response_(base::MakeUnique<BackgroundFetchTestBase::TestResponse>()) {
  response_->headers = make_scoped_refptr(new net::HttpResponseHeaders(
      "HTTP/1.1 " + std::to_string(response_code)));
}

BackgroundFetchTestBase::TestResponseBuilder::~TestResponseBuilder() = default;

BackgroundFetchTestBase::TestResponseBuilder&
BackgroundFetchTestBase::TestResponseBuilder::AddResponseHeader(
    const std::string& name,
    const std::string& value) {
  DCHECK(response_);
  response_->headers->AddHeader(name + ": " + value);
  return *this;
}

BackgroundFetchTestBase::TestResponseBuilder&
BackgroundFetchTestBase::TestResponseBuilder::SetResponseData(
    std::string data) {
  DCHECK(response_);
  response_->data.swap(data);
  return *this;
}

std::unique_ptr<BackgroundFetchTestBase::TestResponse>
BackgroundFetchTestBase::TestResponseBuilder::Build() {
  return std::move(response_);
}

// -----------------------------------------------------------------------------
// RespondingDownloadManager

// Faked download manager that will respond to known HTTP requests with a test-
// defined response. See CreateRequestWithProvidedResponse().
class BackgroundFetchTestBase::RespondingDownloadManager
    : public MockDownloadManager {
 public:
  RespondingDownloadManager() : weak_ptr_factory_(this) {}
  ~RespondingDownloadManager() override = default;

  // Responds to requests to |url| with the given |response|.
  void RegisterResponse(const GURL& url,
                        std::unique_ptr<TestResponse> response) {
    DCHECK_EQ(registered_responses_.count(url), 0u);
    registered_responses_[url] = std::move(response);
  }

  // Called when the Background Fetch system starts a download, all information
  // for which is contained in the |params|.
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override {
    auto iter = registered_responses_.find(params->url());
    if (iter == registered_responses_.end())
      return;

    TestResponse* response = iter->second.get();

    std::unique_ptr<FakeDownloadItem> download_item =
        base::MakeUnique<FakeDownloadItem>();

    download_item->SetURL(params->url());
    download_item->SetUrlChain({params->url()});
    download_item->SetState(DownloadItem::DownloadState::IN_PROGRESS);
    download_item->SetGuid(base::GenerateGUID());
    download_item->SetStartTime(base::Time::Now());
    download_item->SetResponseHeaders(response->headers);

    // Asynchronously invoke the callback set on the |params|, and then continue
    // dealing with the response in this class.
    BrowserThread::PostTaskAndReply(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(params->callback(), download_item.get(),
                       DOWNLOAD_INTERRUPT_REASON_NONE),
        base::BindOnce(&RespondingDownloadManager::DidStartDownload,
                       weak_ptr_factory_.GetWeakPtr(), download_item.get()));

    download_items_.push_back(std::move(download_item));
  }

 private:
  // Called when the download has been "started" by the download manager. This
  // is where we finish the download by sending a single update.
  void DidStartDownload(FakeDownloadItem* download_item) {
    auto iter = registered_responses_.find(download_item->GetURL());
    DCHECK(iter != registered_responses_.end());

    TestResponse* response = iter->second.get();

    download_item->SetState(DownloadItem::DownloadState::COMPLETE);
    download_item->SetEndTime(base::Time::Now());

    base::FilePath response_path;
    if (!temp_directory_.IsValid())
      ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

    // Write the |response|'s data to a temporary file.
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory_.GetPath(),
                                               &response_path));

    ASSERT_NE(-1 /* error */,
              base::WriteFile(response_path, response->data.c_str(),
                              response->data.size()));

    download_item->SetTargetFilePath(response_path);
    download_item->SetReceivedBytes(response->data.size());
    download_item->SetMimeType("text/plain");

    // Notify the Job Controller about the download having been updated.
    download_item->NotifyDownloadUpdated();
  }

  // Map of URL to the response information associated with that URL.
  std::map<GURL, std::unique_ptr<TestResponse>> registered_responses_;

  // Only used to guarantee the lifetime of the created FakeDownloadItems.
  std::vector<std::unique_ptr<FakeDownloadItem>> download_items_;

  // Temporary directory in which successfully downloaded files will be stored.
  base::ScopedTempDir temp_directory_;

  base::WeakPtrFactory<RespondingDownloadManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RespondingDownloadManager);
};

BackgroundFetchTestBase::BackgroundFetchTestBase()
    // Using REAL_IO_THREAD would give better coverage for thread safety, but
    // at time of writing EmbeddedWorkerTestHelper didn't seem to support that.
    : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
      origin_(GURL(kTestOrigin)) {}

BackgroundFetchTestBase::~BackgroundFetchTestBase() {
  DCHECK(set_up_called_);
  DCHECK(tear_down_called_);
}

void BackgroundFetchTestBase::SetUp() {
  download_manager_ = new RespondingDownloadManager();

  // The |download_manager_| ownership is given to the BrowserContext, and the
  // BrowserContext will take care of deallocating it.
  BrowserContext::SetDownloadManagerForTesting(
      browser_context(), base::WrapUnique(download_manager_));

  set_up_called_ = true;
}

void BackgroundFetchTestBase::TearDown() {
  EXPECT_CALL(*download_manager_, Shutdown()).Times(1);

  service_worker_registrations_.clear();
  tear_down_called_ = true;
}

bool BackgroundFetchTestBase::CreateRegistrationId(
    const std::string& tag,
    BackgroundFetchRegistrationId* registration_id) {
  DCHECK(registration_id);
  DCHECK(registration_id->is_null());

  GURL script_url(kTestScriptUrl);

  int64_t service_worker_registration_id = kInvalidServiceWorkerRegistrationId;

  {
    base::RunLoop run_loop;
    embedded_worker_test_helper_.context()->RegisterServiceWorker(
        script_url, ServiceWorkerRegistrationOptions(origin_.GetURL()),
        nullptr /* provider_host */,
        base::Bind(&DidRegisterServiceWorker, &service_worker_registration_id,
                   run_loop.QuitClosure()));

    run_loop.Run();
  }

  if (service_worker_registration_id == kInvalidServiceWorkerRegistrationId) {
    ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
    return false;
  }

  scoped_refptr<ServiceWorkerRegistration> service_worker_registration;

  {
    base::RunLoop run_loop;
    embedded_worker_test_helper_.context()->storage()->FindRegistrationForId(
        service_worker_registration_id, origin_.GetURL(),
        base::Bind(&DidFindServiceWorkerRegistration,
                   &service_worker_registration, run_loop.QuitClosure()));

    run_loop.Run();
  }

  // Wait for the worker to be activated.
  base::RunLoop().RunUntilIdle();

  if (!service_worker_registration) {
    ADD_FAILURE() << "Could not find the new Service Worker registration.";
    return false;
  }

  *registration_id = BackgroundFetchRegistrationId(
      service_worker_registration->id(), origin_, tag);

  service_worker_registrations_.push_back(
      std::move(service_worker_registration));
  return true;
}

ServiceWorkerFetchRequest
BackgroundFetchTestBase::CreateRequestWithProvidedResponse(
    const std::string& method,
    const std::string& url,
    std::unique_ptr<TestResponse> response) {
  GURL gurl(url);

  // Register the |response| with the faked download manager.
  download_manager_->RegisterResponse(gurl, std::move(response));

  // Create a ServiceWorkerFetchRequest request with the same information.
  return ServiceWorkerFetchRequest(gurl, method, ServiceWorkerHeaderMap(),
                                   Referrer(), false /* is_reload */);
}

MockDownloadManager* BackgroundFetchTestBase::download_manager() {
  return download_manager_;
}

}  // namespace content
