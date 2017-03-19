// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kExampleOrigin[] = "https://example.com/";
const char kExampleScriptUrl[] = "https://example.com/sw.js";
const char kExampleTag[] = "my-tag";
const char kExampleTag2[] = "my-second-tag";

// Extension of the EmbeddedWorkerTestHelper that enables instrumentation of the
// events related to the Background Fetch API. Storage for these tests will
// always be kept in memory, as data persistence is tested elsewhere.
class BackgroundFetchEmbeddedWorkerTestHelper
    : public EmbeddedWorkerTestHelper {
 public:
  BackgroundFetchEmbeddedWorkerTestHelper()
      : EmbeddedWorkerTestHelper(base::FilePath() /* in memory */) {}
  ~BackgroundFetchEmbeddedWorkerTestHelper() override = default;

  void set_fail_abort_event(bool fail) { fail_abort_event_ = fail; }

  void set_fail_click_event(bool fail) { fail_click_event_ = fail; }

  const base::Optional<std::string>& last_tag() const { return last_tag_; }
  const base::Optional<mojom::BackgroundFetchState>& last_state() const {
    return last_state_;
  }

 protected:
  void OnBackgroundFetchAbortEvent(
      const std::string& tag,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchAbortEventCallback& callback) override {
    last_tag_ = tag;

    if (fail_abort_event_) {
      callback.Run(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,
                   base::Time::Now());
    } else {
      callback.Run(SERVICE_WORKER_OK, base::Time::Now());
    }
  }

  void OnBackgroundFetchClickEvent(
      const std::string& tag,
      mojom::BackgroundFetchState state,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchBackgroundFetchClickEventCallback& callback) override {
    last_tag_ = tag;
    last_state_ = state;

    if (fail_click_event_) {
      callback.Run(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,
                   base::Time::Now());
    } else {
      callback.Run(SERVICE_WORKER_OK, base::Time::Now());
    }
  }

 private:
  bool fail_abort_event_ = false;
  bool fail_click_event_ = false;

  base::Optional<std::string> last_tag_;
  base::Optional<mojom::BackgroundFetchState> last_state_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchEmbeddedWorkerTestHelper);
};

class BackgroundFetchEventDispatcherTest : public ::testing::Test {
 public:
  BackgroundFetchEventDispatcherTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        event_dispatcher_(embedded_worker_test_helper_.context_wrapper()) {}

  // Creates a new Service Worker registration for a fake origin and scope and
  // returns the ServiceWorkerRegistration instance associated with it.
  scoped_refptr<ServiceWorkerRegistration> RegisterServiceWorker() {
    GURL origin(kExampleOrigin);
    GURL script_url(kExampleScriptUrl);

    int64_t service_worker_registration_id =
        kInvalidServiceWorkerRegistrationId;

    {
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->RegisterServiceWorker(
          origin, script_url, nullptr /* provider_host */,
          base::Bind(
              &BackgroundFetchEventDispatcherTest::DidRegisterServiceWorker,
              base::Unretained(this), &service_worker_registration_id,
              run_loop.QuitClosure()));

      run_loop.Run();
    }

    if (service_worker_registration_id == kInvalidServiceWorkerRegistrationId) {
      ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
      return nullptr;
    }

    scoped_refptr<ServiceWorkerRegistration> service_worker_registration;

    {
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->storage()->FindRegistrationForId(
          service_worker_registration_id, origin,
          base::Bind(&BackgroundFetchEventDispatcherTest::
                         DidFindServiceWorkerRegistration,
                     base::Unretained(this), &service_worker_registration,
                     run_loop.QuitClosure()));

      run_loop.Run();
    }

    // Wait for the worker to be activated.
    base::RunLoop().RunUntilIdle();

    if (!service_worker_registration) {
      ADD_FAILURE() << "Could not find the new Service Worker registration.";
      return nullptr;
    }

    return service_worker_registration;
  }

  BackgroundFetchEmbeddedWorkerTestHelper* test_helpers() {
    return &embedded_worker_test_helper_;
  }

  BackgroundFetchEventDispatcher* dispatcher() { return &event_dispatcher_; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
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

  TestBrowserThreadBundle thread_bundle_;

  BackgroundFetchEmbeddedWorkerTestHelper embedded_worker_test_helper_;
  BackgroundFetchEventDispatcher event_dispatcher_;

  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchEventDispatcherTest);
};

TEST_F(BackgroundFetchEventDispatcherTest, DispatchInvalidRegistration) {
  GURL origin(kExampleOrigin);

  base::RunLoop run_loop;
  dispatcher()->DispatchBackgroundFetchAbortEvent(9042 /* random invalid Id */,
                                                  origin, kExampleTag,
                                                  run_loop.QuitClosure());

  run_loop.Run();

  histogram_tester()->ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_FIND_WORKER, 1);
  histogram_tester()->ExpectBucketCount(
      "BackgroundFetch.EventDispatchFailure.FindWorker.AbortEvent",
      SERVICE_WORKER_ERROR_NOT_FOUND, 1);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchAbortEvent) {
  auto service_worker_registration = RegisterServiceWorker();
  ASSERT_TRUE(service_worker_registration);
  ASSERT_TRUE(service_worker_registration->active_version());

  GURL origin(kExampleOrigin);

  {
    base::RunLoop run_loop;
    dispatcher()->DispatchBackgroundFetchAbortEvent(
        service_worker_registration->id(), origin, kExampleTag,
        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(test_helpers()->last_tag().has_value());
  EXPECT_EQ(kExampleTag, test_helpers()->last_tag().value());

  histogram_tester()->ExpectUniqueSample(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);

  test_helpers()->set_fail_abort_event(true);

  {
    base::RunLoop run_loop;
    dispatcher()->DispatchBackgroundFetchAbortEvent(
        service_worker_registration->id(), origin, kExampleTag2,
        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(test_helpers()->last_tag().has_value());
  EXPECT_EQ(kExampleTag2, test_helpers()->last_tag().value());

  histogram_tester()->ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);
  histogram_tester()->ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_DISPATCH_EVENT, 1);
  histogram_tester()->ExpectUniqueSample(
      "BackgroundFetch.EventDispatchFailure.Dispatch.AbortEvent",
      SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED, 1);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchClickEvent) {
  auto service_worker_registration = RegisterServiceWorker();
  ASSERT_TRUE(service_worker_registration);
  ASSERT_TRUE(service_worker_registration->active_version());

  GURL origin(kExampleOrigin);

  {
    base::RunLoop run_loop;
    dispatcher()->DispatchBackgroundFetchClickEvent(
        service_worker_registration->id(), origin, kExampleTag,
        mojom::BackgroundFetchState::PENDING, run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(test_helpers()->last_tag().has_value());
  EXPECT_EQ(kExampleTag, test_helpers()->last_tag().value());

  ASSERT_TRUE(test_helpers()->last_state().has_value());
  EXPECT_EQ(mojom::BackgroundFetchState::PENDING, test_helpers()->last_state());

  histogram_tester()->ExpectUniqueSample(
      "BackgroundFetch.EventDispatchResult.ClickEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);

  test_helpers()->set_fail_click_event(true);

  {
    base::RunLoop run_loop;
    dispatcher()->DispatchBackgroundFetchClickEvent(
        service_worker_registration->id(), origin, kExampleTag2,
        mojom::BackgroundFetchState::SUCCEEDED, run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(test_helpers()->last_tag().has_value());
  EXPECT_EQ(kExampleTag2, test_helpers()->last_tag().value());

  ASSERT_TRUE(test_helpers()->last_state().has_value());
  EXPECT_EQ(mojom::BackgroundFetchState::SUCCEEDED,
            test_helpers()->last_state());

  histogram_tester()->ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.ClickEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);
  histogram_tester()->ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.ClickEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_DISPATCH_EVENT, 1);
  histogram_tester()->ExpectUniqueSample(
      "BackgroundFetch.EventDispatchFailure.Dispatch.ClickEvent",
      SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED, 1);
}

}  // namespace
}  // namespace content
