// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EMBEDDED_WORKER_TEST_HELPER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EMBEDDED_WORKER_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"

namespace content {

// Extension of the EmbeddedWorkerTestHelper that enables instrumentation of the
// events related to the Background Fetch API. Storage for these tests will
// always be kept in memory, as data persistence is tested elsewhere.
class BackgroundFetchEmbeddedWorkerTestHelper
    : public EmbeddedWorkerTestHelper {
 public:
  BackgroundFetchEmbeddedWorkerTestHelper();
  ~BackgroundFetchEmbeddedWorkerTestHelper() override;

  // Toggles whether the named Service Worker event should fail.
  void set_fail_abort_event(bool fail) { fail_abort_event_ = fail; }
  void set_fail_click_event(bool fail) { fail_click_event_ = fail; }
  void set_fail_fetch_fail_event(bool fail) { fail_fetch_fail_event_ = fail; }
  void set_fail_fetched_event(bool fail) { fail_fetched_event_ = fail; }

  // Sets a base::Callback that should be executed when the named event is ran.
  void set_abort_event_closure(const base::Closure& closure) {
    abort_event_closure_ = closure;
  }
  void set_click_event_closure(const base::Closure& closure) {
    click_event_closure_ = closure;
  }
  void set_fetch_fail_event_closure(const base::Closure& closure) {
    fetch_fail_event_closure_ = closure;
  }
  void set_fetched_event_closure(const base::Closure& closure) {
    fetched_event_closure_ = closure;
  }

  const blink::mojom::BackgroundFetchRegistrationPtr& last_registration()
      const {
    return last_registration_;
  }

 protected:
  // EmbeddedWorkerTestHelper overrides:
  void OnBackgroundFetchAbortEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback
          callback) override;
  void OnBackgroundFetchClickEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback
          callback) override;
  void OnBackgroundFetchFailEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback
          callback) override;
  void OnBackgroundFetchSuccessEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
          callback) override;

 private:
  bool fail_abort_event_ = false;
  bool fail_click_event_ = false;
  bool fail_fetch_fail_event_ = false;
  bool fail_fetched_event_ = false;

  base::Closure abort_event_closure_;
  base::Closure click_event_closure_;
  base::Closure fetch_fail_event_closure_;
  base::Closure fetched_event_closure_;

  blink::mojom::BackgroundFetchRegistrationPtr last_registration_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchEmbeddedWorkerTestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EMBEDDED_WORKER_TEST_HELPER_H_
