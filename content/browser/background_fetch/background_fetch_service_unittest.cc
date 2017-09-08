// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
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
#include "services/network/public/interfaces/fetch_api.mojom.h"

namespace content {
namespace {

const char kExampleId[] = "my-background-fetch";
const char kAlternativeId[] = "my-alternative-fetch";

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
    service_->Fetch(
        registration_id.service_worker_registration_id(),
        registration_id.origin(), registration_id.id(), requests, options,
        base::BindOnce(&BackgroundFetchServiceTest::DidGetRegistration,
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
    service_->Abort(registration_id.service_worker_registration_id(),
                    registration_id.origin(), registration_id.id(),
                    base::BindOnce(&BackgroundFetchServiceTest::DidAbort,
                                   base::Unretained(this),
                                   run_loop.QuitClosure(), out_error));

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
        registration_id.origin(), registration_id.id(),
        base::BindOnce(&BackgroundFetchServiceTest::DidGetRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_registration));

    run_loop.Run();
  }

  // Synchronous wrapper for BackgroundFetchServiceImpl::GetIds().
  void GetIds(const BackgroundFetchRegistrationId& registration_id,
              blink::mojom::BackgroundFetchError* out_error,
              std::vector<std::string>* out_ids) {
    DCHECK(out_error);
    DCHECK(out_ids);

    base::RunLoop run_loop;
    service_->GetIds(
        registration_id.service_worker_registration_id(),
        registration_id.origin(),
        base::BindOnce(&BackgroundFetchServiceTest::DidGetIds,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_ids));

    run_loop.Run();
  }

  // BackgroundFetchTestBase overrides:
  void SetUp() override {
    BackgroundFetchTestBase::SetUp();

    context_ = new BackgroundFetchContext(
        browser_context(),
        make_scoped_refptr(embedded_worker_test_helper()->context_wrapper()));

    service_ = base::MakeUnique<BackgroundFetchServiceImpl>(
        0 /* render_process_id */, context_);
  }

  void TearDown() override {
    BackgroundFetchTestBase::TearDown();

    service_.reset();

    context_ = nullptr;

    // Give pending shutdown operations a chance to finish.
    base::RunLoop().RunUntilIdle();
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

  void DidGetIds(base::Closure quit_closure,
                 blink::mojom::BackgroundFetchError* out_error,
                 std::vector<std::string>* out_ids,
                 blink::mojom::BackgroundFetchError error,
                 const std::vector<std::string>& ids) {
    *out_error = error;
    *out_ids = ids;

    quit_closure.Run();
  }

  scoped_refptr<BackgroundFetchContext> context_;
  std::unique_ptr<BackgroundFetchServiceImpl> service_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchServiceTest);
};

TEST_F(BackgroundFetchServiceTest, FetchInvalidArguments) {
  // This test verifies that the Fetch() function will kill the renderer and
  // return INVALID_ARGUMENT when invalid data is send over the Mojo channel.

  BackgroundFetchOptions options;

  // The `id` must be a non-empty string.
  {
    BackgroundFetchRegistrationId registration_id(
        42 /* service_worker_registration_id */, origin(), "" /* id */);

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
        42 /* service_worker_registration_id */, origin(), kExampleId);

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
  // the active Background Fetch with the same id, and verifies it again.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

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
  EXPECT_EQ(registration.id, kExampleId);
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
  EXPECT_EQ(second_registration.id, kExampleId);
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
  // registration. This should fail with a DUPLICATED_ID error.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

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
  ASSERT_EQ(second_error, blink::mojom::BackgroundFetchError::DUPLICATED_ID);
}

TEST_F(BackgroundFetchServiceTest, FetchSuccessEventDispatch) {
  // This test starts a new Background Fetch, completes the registration, then
  // fetches all files to complete the job, and then verifies that the
  // `backgroundfetched` event will be dispatched with the expected contents.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  // base::RunLoop that we'll run until the event has been dispatched. If this
  // test times out, it means that the event could not be dispatched.
  base::RunLoop event_dispatched_loop;
  embedded_worker_test_helper()->set_fetched_event_closure(
      event_dispatched_loop.QuitClosure());

  std::vector<ServiceWorkerFetchRequest> requests;

  constexpr int kFirstResponseCode = 200;
  constexpr int kSecondResponseCode = 201;
  constexpr int kThirdResponseCode = 200;

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://example.com/funny_cat.txt",
      TestResponseBuilder(kFirstResponseCode)
          .SetResponseData(
              "This text describes a scenario involving a funny cat.")
          .AddResponseHeader("Content-Type", "text/plain")
          .AddResponseHeader("X-Cat", "yes")
          .Build()));

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://example.com/crazy_cat.txt",
      TestResponseBuilder(kSecondResponseCode)
          .SetResponseData(
              "This text describes another scenario that involves a crazy cat.")
          .AddResponseHeader("Content-Type", "text/plain")
          .Build()));

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://chrome.com/accessible_cross_origin_cat.txt",
      TestResponseBuilder(kThirdResponseCode)
          .SetResponseData("This cat originates from another origin.")
          .AddResponseHeader("Access-Control-Allow-Origin", "*")
          .AddResponseHeader("Content-Type", "text/plain")
          .Build()));

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

  ASSERT_TRUE(embedded_worker_test_helper()->last_id().has_value());
  EXPECT_EQ(kExampleId, embedded_worker_test_helper()->last_id().value());

  ASSERT_TRUE(embedded_worker_test_helper()->last_fetches().has_value());

  std::vector<BackgroundFetchSettledFetch> fetches =
      embedded_worker_test_helper()->last_fetches().value();
  ASSERT_EQ(fetches.size(), requests.size());

  for (size_t i = 0; i < fetches.size(); ++i) {
    ASSERT_EQ(fetches[i].request.url, requests[i].url);
    EXPECT_EQ(fetches[i].request.method, requests[i].method);

    EXPECT_EQ(fetches[i].response.url_list[0], fetches[i].request.url);
    EXPECT_EQ(fetches[i].response.response_type,
              network::mojom::FetchResponseType::kDefault);

    switch (i) {
      case 0:
        EXPECT_EQ(fetches[i].response.status_code, kFirstResponseCode);
        EXPECT_EQ(fetches[i].response.headers.count("Content-Type"), 1u);
        EXPECT_EQ(fetches[i].response.headers.count("X-Cat"), 1u);
        break;
      case 1:
        EXPECT_EQ(fetches[i].response.status_code, kSecondResponseCode);
        EXPECT_EQ(fetches[i].response.headers.count("Content-Type"), 1u);
        EXPECT_EQ(fetches[i].response.headers.count("X-Cat"), 0u);
        break;
      case 2:
        EXPECT_EQ(fetches[i].response.status_code, kThirdResponseCode);
        EXPECT_EQ(fetches[i].response.headers.count("Content-Type"), 1u);
        EXPECT_EQ(fetches[i].response.headers.count("X-Cat"), 0u);
        break;
      default:
        NOTREACHED();
    }

    // TODO(peter): change-detector tests for unsupported properties.
    EXPECT_EQ(fetches[i].response.error,
              blink::kWebServiceWorkerResponseErrorUnknown);

    // Verify that all properties have a sensible value.
    EXPECT_FALSE(fetches[i].response.response_time.is_null());

    // Verify that the response blobs have been populated. We cannot consume
    // their data here since the handles have already been released.
    ASSERT_FALSE(fetches[i].response.blob_uuid.empty());
    ASSERT_GT(fetches[i].response.blob_size, 0u);
  }
}

TEST_F(BackgroundFetchServiceTest, FetchFailEventDispatch) {
  // This test verifies that the fail event will be fired when a response either
  // has a non-OK status code, or the response cannot be accessed due to CORS.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  // base::RunLoop that we'll run until the event has been dispatched. If this
  // test times out, it means that the event could not be dispatched.
  base::RunLoop event_dispatched_loop;
  embedded_worker_test_helper()->set_fetch_fail_event_closure(
      event_dispatched_loop.QuitClosure());

  std::vector<ServiceWorkerFetchRequest> requests;

  constexpr int kFirstResponseCode = 404;
  constexpr int kSecondResponseCode = 200;

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://example.com/not_existing_cat.txt",
      TestResponseBuilder(kFirstResponseCode).Build()));

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://chrome.com/inaccessible_cross_origin_cat.txt",
      TestResponseBuilder(kSecondResponseCode)
          .SetResponseData(
              "This is a cross-origin response not accessible to the reader.")
          .AddResponseHeader("Content-Type", "text/plain")
          .Build()));

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

  ASSERT_TRUE(embedded_worker_test_helper()->last_id().has_value());
  EXPECT_EQ(kExampleId, embedded_worker_test_helper()->last_id().value());

  ASSERT_TRUE(embedded_worker_test_helper()->last_fetches().has_value());

  std::vector<BackgroundFetchSettledFetch> fetches =
      embedded_worker_test_helper()->last_fetches().value();
  ASSERT_EQ(fetches.size(), requests.size());

  for (size_t i = 0; i < fetches.size(); ++i) {
    ASSERT_EQ(fetches[i].request.url, requests[i].url);
    EXPECT_EQ(fetches[i].request.method, requests[i].method);

    EXPECT_EQ(fetches[i].response.url_list[0], fetches[i].request.url);
    EXPECT_EQ(fetches[i].response.response_type,
              network::mojom::FetchResponseType::kDefault);

    switch (i) {
      case 0:
        EXPECT_EQ(fetches[i].response.status_code, 404);
        break;
      case 1:
        EXPECT_EQ(fetches[i].response.status_code, 0);
        break;
      default:
        NOTREACHED();
    }

    EXPECT_TRUE(fetches[i].response.headers.empty());
    EXPECT_TRUE(fetches[i].response.blob_uuid.empty());
    EXPECT_EQ(fetches[i].response.blob_size, 0u);
    EXPECT_FALSE(fetches[i].response.response_time.is_null());

    // TODO(peter): change-detector tests for unsupported properties.
    EXPECT_EQ(fetches[i].response.error,
              blink::kWebServiceWorkerResponseErrorUnknown);
    EXPECT_TRUE(fetches[i].response.cors_exposed_header_names.empty());
  }
}

TEST_F(BackgroundFetchServiceTest, Abort) {
  // This test starts a new Background Fetch, completes the registration, and
  // then aborts the Background Fetch mid-process. Tests all of StartFetch(),
  // GetActiveFetches() and GetActiveIdsForServiceWorkerRegistration().

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

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
  // Wait for the response of the Mojo IPC to dispatch
  // BackgroundFetchAbortEvent.
  base::RunLoop().RunUntilIdle();

  blink::mojom::BackgroundFetchError second_error;
  BackgroundFetchRegistration second_registration;

  // Now try to get the created registration, which is expected to fail.
  ASSERT_NO_FATAL_FAILURE(
      GetRegistration(registration_id, &second_error, &second_registration));
  ASSERT_EQ(second_error, blink::mojom::BackgroundFetchError::INVALID_ID);
}

TEST_F(BackgroundFetchServiceTest, AbortInvalidArguments) {
  // This test verifies that the Abort() function will kill the renderer and
  // return INVALID_ARGUMENT when invalid data is send over the Mojo channel.

  BackgroundFetchRegistrationId registration_id(
      42 /* service_worker_registration_id */, origin(), "" /* id */);

  blink::mojom::BackgroundFetchError error;

  ASSERT_NO_FATAL_FAILURE(Abort(registration_id, &error));
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
}

TEST_F(BackgroundFetchServiceTest, AbortInvalidId) {
  // This test verifies that aborting a Background Fetch registration with a
  // id that does not correspond to an active fetch kindly tells us so.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  // Deliberate do *not* create a fetch for the |registration_id|.

  blink::mojom::BackgroundFetchError error;

  ASSERT_NO_FATAL_FAILURE(Abort(registration_id, &error));
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);
}

TEST_F(BackgroundFetchServiceTest, AbortEventDispatch) {
  // Tests that the `backgroundfetchabort` event will be fired when a Background
  // Fetch registration has been aborted by either the user or developer.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  // base::RunLoop that we'll run until the event has been dispatched. If this
  // test times out, it means that the event could not be dispatched.
  base::RunLoop event_dispatched_loop;
  embedded_worker_test_helper()->set_abort_event_closure(
      event_dispatched_loop.QuitClosure());

  constexpr int kResponseCode = 200;

  std::vector<ServiceWorkerFetchRequest> requests;
  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", "https://example.com/funny_cat.txt",
      TestResponseBuilder(kResponseCode)
          .SetResponseData("Random data about a funny cat.")
          .Build()));

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

  // Immediately abort the request created for the |registration_id|. Then wait
  // for the `backgroundfetchabort` event to have been invoked.
  {
    blink::mojom::BackgroundFetchError error;

    ASSERT_NO_FATAL_FAILURE(Abort(registration_id, &error));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  event_dispatched_loop.Run();

  ASSERT_TRUE(embedded_worker_test_helper()->last_id().has_value());
  EXPECT_EQ(kExampleId, embedded_worker_test_helper()->last_id().value());
}

TEST_F(BackgroundFetchServiceTest, GetIds) {
  // This test verifies that the list of active ids can be retrieved from the
  // service for a given Service Worker, as extracted from a registration.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  BackgroundFetchRegistrationId second_registration_id;
  ASSERT_TRUE(CreateRegistrationId(kAlternativeId, &second_registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  requests.emplace_back();  // empty, but valid

  BackgroundFetchOptions options;

  // Verify that there are no active ids yet.
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> ids;

    ASSERT_NO_FATAL_FAILURE(GetIds(registration_id, &error, &ids));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(ids.size(), 0u);
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
    std::vector<std::string> ids;

    ASSERT_NO_FATAL_FAILURE(GetIds(registration_id, &error, &ids));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], kExampleId);
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
    std::vector<std::string> ids;

    ASSERT_NO_FATAL_FAILURE(GetIds(registration_id, &error, &ids));
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(ids.size(), 2u);

    // We make no guarantees about ordering of the ids.
    const bool has_example_id = ids[0] == kExampleId || ids[1] == kExampleId;
    const bool has_alternative_id =
        ids[0] == kAlternativeId || ids[1] == kAlternativeId;

    EXPECT_TRUE(has_example_id);
    EXPECT_TRUE(has_alternative_id);
  }
}

}  // namespace
}  // namespace content
