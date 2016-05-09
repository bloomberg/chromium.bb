// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <utility>

#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

RequestCoordinator::RequestCoordinator(
    std::unique_ptr<OfflinerPolicy> policy,
    std::unique_ptr<OfflinerFactory> factory) {
  // Do setup as needed.
  // TODO(petewil): Assert policy not null.
  policy_ = std::move(policy);
  factory_ = std::move(factory);
}

RequestCoordinator::~RequestCoordinator() {}

bool RequestCoordinator::SavePageLater(
    const GURL& url, const ClientId& client_id) {

  // TODO(petewil): We need a robust scheme for allocating new IDs.
  // TODO(petewil): Build a SavePageRequest.
  // TODO(petewil): Put the request on the request queue, nudge the scheduler.

  return true;
}

bool RequestCoordinator::StartProcessing(
    const ProcessingDoneCallback& callback) {
  return false;
}

void RequestCoordinator::StopProcessing() {
}

}  // namespace offline_pages
