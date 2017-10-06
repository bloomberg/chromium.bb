// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_embedded_worker_test_helper.h"

#include "base/callback.h"
#include "base/time/time.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"

namespace content {

BackgroundFetchEmbeddedWorkerTestHelper::
    BackgroundFetchEmbeddedWorkerTestHelper()
    : EmbeddedWorkerTestHelper(base::FilePath() /* in memory */) {}

BackgroundFetchEmbeddedWorkerTestHelper::
    ~BackgroundFetchEmbeddedWorkerTestHelper() = default;

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent(
    const std::string& id,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchAbortEventCallback callback) {
  last_id_ = id;

  if (fail_abort_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::Time::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::Time::Now());
  }

  if (abort_event_closure_)
    abort_event_closure_.Run();
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent(
    const std::string& id,
    mojom::BackgroundFetchState state,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchClickEventCallback callback) {
  last_id_ = id;
  last_state_ = state;

  if (fail_click_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::Time::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::Time::Now());
  }

  if (click_event_closure_)
    click_event_closure_.Run();
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent(
    const std::string& id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchFailEventCallback callback) {
  last_id_ = id;
  last_fetches_ = fetches;

  if (fail_fetch_fail_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::Time::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::Time::Now());
  }

  if (fetch_fail_event_closure_)
    fetch_fail_event_closure_.Run();
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchedEvent(
    const std::string& id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    mojom::ServiceWorkerEventDispatcher::DispatchBackgroundFetchedEventCallback
        callback) {
  last_id_ = id;
  last_fetches_ = fetches;

  if (fail_fetched_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::Time::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::Time::Now());
  }

  if (fetched_event_closure_)
    fetched_event_closure_.Run();
}

}  // namespace content
