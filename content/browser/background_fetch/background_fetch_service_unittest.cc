// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_embedded_worker_test_helper.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"

namespace content {
namespace {

const char kExampleTag[] = "my-background-fetch";
const char kAlternativeTag[] = "my-alternative-fetch";

// Faked download manager that will respond to known HTTP requests with a test-
// defined response. See CreateRequestWithProvidedResponse().
class RespondingDownloadManager : public MockDownloadManager {
 public:
  RespondingDownloadManager() : weak_ptr_factory_(this) {}
  ~RespondingDownloadManager() override = default;

  // Responds to requests to |url| with the |status_code| and |response_text|.
  void RegisterResponse(const GURL& url,
                        int status_code,
                        const std::string& response_text) {
    DCHECK_EQ(registered_responses_.count(url), 0u);
    registered_responses_[url] = std::make_pair(status_code, response_text);
  }

  // Called when the Background Fetch system starts a download, all information
  // for which is contained in the |params|.
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override {
    auto iter = registered_responses_.find(params->url());
    if (iter == registered_responses_.end())
      return;

    std::unique_ptr<FakeDownloadItem> download_item =
        base::MakeUnique<FakeDownloadItem>();

    download_item->SetURL(params->url());
    download_item->SetState(DownloadItem::DownloadState::IN_PROGRESS);
    download_item->SetGuid(base::GenerateGUID());
    download_item->SetStartTime(base::Time());

    // Asynchronously invoke the callback set on the |params|, and then continue
    // dealing with the response in this class.
    BrowserThread::PostTaskAndReply(
        BrowserThread::UI, FROM_HERE,
        base::Bind(params->callback(), download_item.get(),
                   DOWNLOAD_INTERRUPT_REASON_NONE),
        base::Bind(&RespondingDownloadManager::DidStartDownload,
                   weak_ptr_factory_.GetWeakPtr(), download_item.get()));

    download_items_.push_back(std::move(download_item));
  }

 private:
  // Called when the download has been "started" by the download manager. This
  // is where we finish the download by sending a single update.
  void DidStartDownload(FakeDownloadItem* download_item) {
    auto iter = registered_responses_.find(download_item->GetURL());
    DCHECK(iter != registered_responses_.end());

    const ResponseInfo& response_info = iter->second;

    download_item->SetState(DownloadItem::DownloadState::COMPLETE);
    download_item->SetEndTime(base::Time());

    // TODO(peter): Set response body, status code and so on.
    download_item->SetReceivedBytes(response_info.second.size());

    // Notify the Job Controller about the download having been updated.
    download_item->NotifyDownloadUpdated();
  }

  using ResponseInfo =
      std::pair<int /* status_code */, std::string /* response_text */>;

  // Map of URL to the response information associated with that URL.
  std::map<GURL, ResponseInfo> registered_responses_;

  // Only used to guarantee the lifetime of the created FakeDownloadItems.
  std::vector<std::unique_ptr<FakeDownloadItem>> download_items_;

  base::WeakPtrFactory<RespondingDownloadManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RespondingDownloadManager);
};

IconDefinition CreateIcon(std::string src,
                          std::string sizes,
                          std::string type) {
  IconDefinition icon;
  icon.src = std::move(src);
  icon.sizes = std::move(sizes);
  icon.type = std::move(type);

  return icon;
}

class BackgroundFetchServiceTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchServiceTest() = default;
  ~BackgroundFetchServiceTest() override = default;

  // Synchronous wrapper for BackgroundFetchServiceImpl::Fetch().
  // Should be wrapped in ASSERT_NO_FATAL_FAILURE().
  void Fetch(const BackgroundFetchRegistrationId& registration_id,
             const std::vector<ServiceWorkerFetchRequest>& requests,
             const BackgroundFetchOptions& options,
             blink::mojom::BackgroundFetchError* out_error,
             BackgroundFetchRegistration* out_registration) {
    DCHECK(out_error);
    DCHECK(out_registration);

    base::RunLoop run_loop;
    service_->Fetch(registration_id.service_worker_registration_id(),
                    registration_id.origin(), registration_id.tag(), requests,
                    options,
                    base::Bind(&BackgroundFetchServiceTest::DidGetRegistration,
                               base::Unretained(this), run_loop.QuitClosure(),
                               out_error, out_registration));

    run_loop.Run();
  }

  // TODO(harkness): Add tests for UpdateUI() when its functionality has been
  // implemented.

  // Synchronous wrapper for BackgroundFetchServiceImpl::Abort().
  // Should be wrapped in ASSERT_NO_FATAL_FAILURE().
  void Abort(const BackgroundFetchRegistrationId& registration_id,
             blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    service_->Abort(
        registration_id.service_worker_registration_id(),
        registration_id.origin(), registration_id.tag(),
        base::Bind(&BackgroundFetchServiceTest::DidAbort,
                   base::Unretained(this), run_loop.QuitClosure(), out_error));

    run_loop.Run();
  }

  // Synchronous wrapper for BackgroundFetchServiceImpl::GetRegistration().
  // Should be wrapped in ASSERT_NO_FATAL_FAILURE().
  void GetRegistration(const BackgroundFetchRegistrationId& registration_id,
                       blink::mojom::BackgroundFetchError* out_error,
                       BackgroundFetchRegistration* out_registration) {
    DCHECK(out_error);
    DCHECK(out_registration);

    base::RunLoop run_loop;
    service_->GetRegistration(
        registration_id.service_worker_registration_id(),
        registration_id.origin(), registration_id.tag(),
        base::Bind(&BackgroundFetchServiceTest::DidGetRegistration,
                   base::Unretained(this), run_loop.QuitClosure(), out_error,
                   out_registration));

    run_loop.Run();
  }

  // Synchronous wrapper for BackgroundFetchServiceImpl::GetTags().
  void GetTags(const BackgroundFetchRegistrationId& registration_id,
               blink::mojom::BackgroundFetchError* out_error,
               std::vector<std::string>* out_tags) {
    DCHECK(out_error);
    DCHECK(out_tags);

    base::RunLoop run_loop;
    service_->GetTags(registration_id.service_worker_registration_id(),
                      registration_id.origin(),
                      base::Bind(&BackgroundFetchServiceTest::DidGetTags,
                                 base::Unretained(this), run_loop.QuitClosure(),
                                 out_error, out_tags));

    run_loop.Run();
  }

  // Creates a ServiceWorkerFetchRequest instance for the given details and
  // provides a faked response with |status_code| and |response_text| to the
  // download manager, that will resolve the download with that information.
  ServiceWorkerFetchRequest CreateRequestWithProvidedResponse(
      const std::string& method,
      const std::string& url_string,
      int status_code,
      const std::string& response_text) {
    GURL url(url_string);

    // Register the |status_code| and |response_text| with the download manager.
    download_manager_->RegisterResponse(GURL(url_string), status_code,
                                        response_text);

    // Create a ServiceWorkerFetchRequest request with the same information.
    return ServiceWorkerFetchRequest(url, method, ServiceWorkerHeaderMap(),
                                     Referrer(), false /* is_reload */);
  }

  // BackgroundFetchTestBase overrides:
  void SetUp() override {
    BackgroundFetchTestBase::SetUp();

    download_manager_ = new RespondingDownloadManager();

    // The |download_manager_| ownership is given to the BrowserContext, and the
    // BrowserContext will take care of deallocating it.
    BrowserContext::SetDownloadManagerForTesting(browser_context(),
                                                 download_manager_);

    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetDefaultStoragePartition(browser_context()));

    context_ = new BackgroundFetchContext(
        browser_context(), storage_partition,
        make_scoped_refptr(embedded_worker_test_helper()->context_wrapper()));
    service_ = base::MakeUnique<BackgroundFetchServiceImpl>(
        0 /* render_process_id */, context_);
  }

  void TearDown() override {
    service_.reset();

    context_->Shutdown();
    context_ = nullptr;

    // Give pending shutdown operations a chance to finish.
    base::RunLoop().RunUntilIdle();

    BackgroundFetchTestBase::TearDown();
  }

 private:
  void DidGetRegistration(
      base::Closure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      BackgroundFetchRegistration* out_registration,
      blink::mojom::BackgroundFetchError error,
      const base::Optional<content::BackgroundFetchRegistration>&
          registration) {
    *out_error = error;
    if (registration)
      *out_registration = registration.value();

    quit_closure.Run();
  }

  void DidAbort(base::Closure quit_closure,
                blink::mojom::BackgroundFetchError* out_error,
                blink::mojom::BackgroundFetchError error) {
    *out_error = error;

    quit_closure.Run();
  }

  void DidGetTags(base::Closure quit_closure,
                  blink::mojom::BackgroundFetchError* out_error,
                  std::vector<std::string>* out_tags,
                  blink::mojom::BackgroundFetchError error,
                  const std::vector<std::string>& tags) {
    *out_error = error;
    *out_tags = tags;

    quit_closure.Run();
  }

  scoped_refptr<BackgroundFetchContext> context_;
  std::unique_ptr<BackgroundFetchServiceImpl> service_;

  RespondingDownloadManager* download_manager_;  // BrowserContext owned

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchServiceTest);
};

TEST_F(BackgroundFetchServiceTest, FetchInvalidArguments) {
  // This test verifies that the Fetch() function will kill the renderer and
  // return INVALID_ARGUMENT when invalid data is send over the Mojo channel.

  BackgroundFetchOptions options;

  // The `tag` must be a non-empty string.
  {
    BackgroundFetchRegistrationId registration_id(
        42 /* service_worker_registration_id */, origin(), "" /* tag */);

    std::vector<ServiceWorkerFetchRequest> requests;
    requests.emplace_back();  // empty, but valid

    blink::mojom::BackgroundFetchError error;
    BackgroundFetchRegistration registration;

    ASSERT_NO_FATAL_FAILURE(
        Fetch(registration_id, requests, options, &error, &registration));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
  }

  // At least a single ServiceWorkerFetchRequest must be given.
  {
    BackgroundFetchRegistrationId registration_id(
        42 /* service_worker_registration_id */, origin(), kExampleTag);

    std::vector<ServiceWorkerFetchRequest> requests;
    // |requests| has deliberately been left empty.

    blink::mojom::BackgroundFetchError error;
    BackgroundFetchRegistration registration;

    ASSERT_NO_FATAL_FAILURE(
        Fetch(registration_id, requests, options, &error, &registration));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
  }
}

TEST_F(BackgroundFetchServiceTest, FetchRegistrationProperties) {
  // This test starts a new Background Fetch and verifies that the returned
  // BackgroundFetchRegistration object matches the given options. Then gets
  // the active Background Fetch with the same tag, and verifies it again.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleTag, &registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  requests.emplace_back();  // empty, but valid

  BackgroundFetchOptions options;
  options.icons.push_back(CreateIcon("funny_cat.png", "256x256", "image/png"));
  options.icons.push_back(CreateIcon("silly_cat.gif", "512x512", "image/gif"));
  options.title = "My Background Fetch!";
  options.total_download_size = 9001;

  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistration registration;

  ASSERT_NO_FATAL_FAILURE(
      Fetch(registration_id, requests, options, &error, &registration));
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // The |registration| should reflect the options given in |options|.
  EXPECT_EQ(registration.tag, kExampleTag);
  ASSERT_EQ(registration.icons.size(), options.icons.size());

  for (size_t i = 0; i < registration.icons.size(); ++i) {
    EXPECT_EQ(registration.icons[i].src, options.icons[i].src);
    EXPECT_EQ(registration.icons[i].sizes, options.icons[i].sizes);
    EXPECT_EQ(registration.icons[i].type, options.icons[i].type);
  }

  EXPECT_EQ(registration.title, options.title);
  EXPECT_EQ(registration.total_download_size, options.total_download_size);

  blink::mojom::BackgroundFetchError second_error;
  BackgroundFetchRegistration second_registration;

  ASSERT_NO_FATAL_FAILURE(
      GetRegistration(registration_id, &second_error, &second_registration));
  ASSERT_EQ(second_error, blink::mojom::BackgroundFetchError::NONE);

  // The |second_registration| should reflect the options given in |options|.
  EXPECT_EQ(second_registration.tag, kExampleTag);
  ASSERT_EQ(second_registration.icons.size(), options.icons.size());

  for (size_t i = 0; i < second_registration.icons.size(); ++i) {
    EXPECT_EQ(second_registration.icons[i].src, options.icons[i].src);
    EXPECT_EQ(second_registration.icons[i].sizes, options.icons[i].sizes);
    EXPECT_EQ(second_registration.icons[i].type, options.icons[i].type);
  }

  EXPECT_EQ(second_registration.title, options.title);
  EXPECT_EQ(second_registration.total_download_size,
            options.total_download_size);
}

TEST_F(BackgroundFetchServiceTest, FetchDuplicatedRegistrationFailure) {
  // This tests starts a new Background Fetch, verifies that a registration was
  // successfully created, and then tries to start a second fetch for the same
  // registration. This should fail with a DUPLICATED_TAG error.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleTag, &registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  requests.emplace_back();  // empty, but valid

  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistration registration;

  // Create the first registration. This must succeed.
  ASSERT_NO_FATAL_FAILURE(
      Fetch(registration_id, requests, options, &error, &registration));
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  blink::mojom::BackgroundFetchError second_error;
  BackgroundFetchRegistration second_registration;

  // Create the second registration with the same data. This must fail.
  ASSERT_NO_FATAL_FAILURE(Fetch(registration_id, requests, options,
                                &second_error, &second_registration));
  ASSERT_EQ(second_error, blink::mojom::BackgroundFetchError::DUPLICATED_TAG);
}

TEST_F(BackgroundFetchServiceTest, FetchSuccessEventDispatch) {
  // This test starts a new Background Fetch, completes the registration, then
  // fetches all files to complete the job, and then verifies that the
  // `backgroundfetched` event will be dispatched with the expected contents.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleTag, &registration_id));

  // base::RunLoop that we'll run until the event has been dispatched. If this
  // test times out, it means that the event could not be dispatched.
  base::RunLoop event_dispatched_loop;
  embedded_worker_test_helper()->set_fetched_event_closure(
      event_dispatched_loop.QuitClosure());

  std::vector<ServiceWorkerFetchRequest> requests;
  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://example.com/funny_cat.txt", 200 /* status_code */,
      "This text describes a scenario involving a funny cat."));
  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://example.com/crazy_cat.txt", 200 /* status_code */,
      "This text descrubes a scenario involving a crazy cat."));

  // Create the registration with the given |requests|.
  {
    BackgroundFetchOptions options;

    blink::mojom::BackgroundFetchError error;
    BackgroundFetchRegistration registration;

    // Create the first registration. This must succeed.
    ASSERT_NO_FATAL_FAILURE(
        Fetch(registration_id, requests, options, &error, &registration));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Spin the |event_dispatched_loop| to wait for the dispatched event.
  event_dispatched_loop.Run();

  ASSERT_TRUE(embedded_worker_test_helper()->last_tag().has_value());
  EXPECT_EQ(kExampleTag, embedded_worker_test_helper()->last_tag().value());

  ASSERT_TRUE(embedded_worker_test_helper()->last_fetches().has_value());

  std::vector<BackgroundFetchSettledFetch> fetches =
      embedded_worker_test_helper()->last_fetches().value();
  ASSERT_EQ(fetches.size(), requests.size());

  for (size_t i = 0; i < fetches.size(); ++i) {
    ASSERT_EQ(fetches[i].request.url, requests[i].url);
    EXPECT_EQ(fetches[i].request.method, requests[i].method);

    EXPECT_EQ(fetches[i].request.url, fetches[i].response.url_list[0]);

    // TODO(peter): change-detector tests for unsupported properties.
    EXPECT_EQ(fetches[i].response.status_code, 0);
    EXPECT_TRUE(fetches[i].response.status_text.empty());
    EXPECT_EQ(fetches[i].response.response_type,
              blink::WebServiceWorkerResponseTypeOpaque);
    EXPECT_TRUE(fetches[i].response.headers.empty());
    EXPECT_EQ(fetches[i].response.error,
              blink::WebServiceWorkerResponseErrorUnknown);
    EXPECT_TRUE(fetches[i].response.response_time.is_null());

    // TODO(peter): Change-detector tests for when bodies are supported.
    EXPECT_TRUE(fetches[i].response.blob_uuid.empty());
    EXPECT_EQ(fetches[i].response.blob_size, 0u);
  }
}

TEST_F(BackgroundFetchServiceTest, Abort) {
  // This test starts a new Background Fetch, completes the registration, and
  // then aborts the Background Fetch mid-process. Tests all of StartFetch(),
  // GetActiveFetches() and GetActiveTagsForServiceWorkerRegistration().

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleTag, &registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  requests.emplace_back();  // empty, but valid

  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistration registration;

  // Create the registration. This must succeed.
  ASSERT_NO_FATAL_FAILURE(
      Fetch(registration_id, requests, options, &error, &registration));
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  blink::mojom::BackgroundFetchError abort_error;

  // Immediately abort the registration. This also is expected to succeed.
  ASSERT_NO_FATAL_FAILURE(Abort(registration_id, &abort_error));
  ASSERT_EQ(abort_error, blink::mojom::BackgroundFetchError::NONE);

  blink::mojom::BackgroundFetchError second_error;
  BackgroundFetchRegistration second_registration;

  // Now try to get the created registration, which is expected to fail.
  ASSERT_NO_FATAL_FAILURE(
      GetRegistration(registration_id, &second_error, &second_registration));
  ASSERT_EQ(second_error, blink::mojom::BackgroundFetchError::INVALID_TAG);
}

TEST_F(BackgroundFetchServiceTest, AbortInvalidArguments) {
  // This test verifies that the Abort() function will kill the renderer and
  // return INVALID_ARGUMENT when invalid data is send over the Mojo channel.

  BackgroundFetchRegistrationId registration_id(
      42 /* service_worker_registration_id */, origin(), "" /* tag */);

  blink::mojom::BackgroundFetchError error;

  ASSERT_NO_FATAL_FAILURE(Abort(registration_id, &error));
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
}

TEST_F(BackgroundFetchServiceTest, AbortInvalidTag) {
  // This test verifies that aborting a Background Fetch registration with a
  // tag that does not correspond to an active fetch kindly tells us so.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleTag, &registration_id));

  // Deliberate do *not* create a fetch for the |registration_id|.

  blink::mojom::BackgroundFetchError error;

  ASSERT_NO_FATAL_FAILURE(Abort(registration_id, &error));
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_TAG);
}

TEST_F(BackgroundFetchServiceTest, GetTags) {
  // This test verifies that the list of active tags can be retrieved from the
  // service for a given Service Worker, as extracted from a registration.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleTag, &registration_id));

  BackgroundFetchRegistrationId second_registration_id;
  ASSERT_TRUE(CreateRegistrationId(kAlternativeTag, &second_registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  requests.emplace_back();  // empty, but valid

  BackgroundFetchOptions options;

  // Verify that there are no active tags yet.
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> tags;

    ASSERT_NO_FATAL_FAILURE(GetTags(registration_id, &error, &tags));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(tags.size(), 0u);
  }

  // Start the Background Fetch for the |registration_id|.
  {
    blink::mojom::BackgroundFetchError error;
    BackgroundFetchRegistration registration;

    ASSERT_NO_FATAL_FAILURE(
        Fetch(registration_id, requests, options, &error, &registration));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that there is a single active fetch (the one we just started).
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> tags;

    ASSERT_NO_FATAL_FAILURE(GetTags(registration_id, &error, &tags));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(tags.size(), 1u);
    EXPECT_EQ(tags[0], kExampleTag);
  }

  // Start the Background Fetch for the |second_registration_id|.
  {
    blink::mojom::BackgroundFetchError error;
    BackgroundFetchRegistration registration;

    ASSERT_NO_FATAL_FAILURE(Fetch(second_registration_id, requests, options,
                                  &error, &registration));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that there are two active fetches.
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> tags;

    ASSERT_NO_FATAL_FAILURE(GetTags(registration_id, &error, &tags));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(tags.size(), 2u);

    // We make no guarantees about ordering of the tags.
    const bool has_example_tag =
        tags[0] == kExampleTag || tags[1] == kExampleTag;
    const bool has_alternative_tag =
        tags[0] == kAlternativeTag || tags[1] == kAlternativeTag;

    EXPECT_TRUE(has_example_tag);
    EXPECT_TRUE(has_alternative_tag);
  }
}

}  // namespace
}  // namespace content
