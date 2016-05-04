// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

// TODO(dougarnett): How to inject Offliner factories and policy objects.
RequestCoordinator::RequestCoordinator() {
  // Do setup as needed.
}

RequestCoordinator::~RequestCoordinator() {
  // Do cleanup as needed.
}

bool RequestCoordinator::SavePageLater(const SavePageRequest& request) {
  return true;
}

bool RequestCoordinator::StartProcessing(
    const ProcessingDoneCallback& callback) {
  return false;
}

void RequestCoordinator::StopProcessing() {
}

}  // namespace offline_pages
