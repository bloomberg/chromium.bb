// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_embedded_worker_test_helper.h"

#include "base/callback.h"
#include "base/time/time.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

BackgroundFetchEmbeddedWorkerTestHelper::
    BackgroundFetchEmbeddedWorkerTestHelper()
    : EmbeddedWorkerTestHelper(base::FilePath() /* in memory */) {}

BackgroundFetchEmbeddedWorkerTestHelper::
    ~BackgroundFetchEmbeddedWorkerTestHelper() = default;

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent(
    const std::string& tag,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchAbortEventCallback& callback) {
  last_tag_ = tag;

  if (fail_abort_event_) {
    callback.Run(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,
                 base::Time::Now());
  } else {
    callback.Run(SERVICE_WORKER_OK, base::Time::Now());
  }
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent(
    const std::string& tag,
    mojom::BackgroundFetchState state,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchClickEventCallback& callback) {
  last_tag_ = tag;
  last_state_ = state;

  if (fail_click_event_) {
    callback.Run(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,
                 base::Time::Now());
  } else {
    callback.Run(SERVICE_WORKER_OK, base::Time::Now());
  }
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent(
    const std::string& tag,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchFailEventCallback& callback) {
  last_tag_ = tag;
  last_fetches_ = fetches;

  if (fail_fail_event_) {
    callback.Run(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,
                 base::Time::Now());
  } else {
    callback.Run(SERVICE_WORKER_OK, base::Time::Now());
  }
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchedEvent(
    const std::string& tag,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchedEventCallback& callback) {
  last_tag_ = tag;
  last_fetches_ = fetches;

  if (fail_fetched_event_) {
    callback.Run(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,
                 base::Time::Now());
  } else {
    callback.Run(SERVICE_WORKER_OK, base::Time::Now());
  }
}

}  // namespace content
