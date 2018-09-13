// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_update_checker.h"

namespace content {

ServiceWorkerUpdateChecker::ServiceWorkerUpdateChecker(
    std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare)
    : scripts_to_compare_(std::move(scripts_to_compare)), weak_factory_(this) {}

ServiceWorkerUpdateChecker::~ServiceWorkerUpdateChecker() = default;

void ServiceWorkerUpdateChecker::Start(UpdateStatusCallback callback) {
  DCHECK(!scripts_to_compare_.empty());
  callback_ = std::move(callback);
  CheckOneScript();
}

void ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished(
    bool is_script_changed) {
  running_checker_.reset();
  if (is_script_changed) {
    // Found an updated script. Stop the comparison of scripts here and
    // return to ServiceWorkerRegisterJob to continue the update.
    // Note that running |callback_| will delete |this|.
    std::move(callback_).Run(true);
    return;
  }
  CheckOneScript();
}

void ServiceWorkerUpdateChecker::CheckOneScript() {
  DCHECK_LE(scripts_compared_, scripts_to_compare_.size());
  if (scripts_compared_ == scripts_to_compare_.size()) {
    // None of the scripts that went through the comparison had any updates.
    // Note that running |callback_| will delete |this|.
    std::move(callback_).Run(false);
    return;
  }

  ServiceWorkerDatabase::ResourceRecord script =
      scripts_to_compare_[scripts_compared_++];
  DCHECK_NE(kInvalidServiceWorkerResourceId, script.resource_id)
      << "All the target scripts should be stored in the storage.";

  running_checker_ = std::make_unique<ServiceWorkerSingleScriptUpdateChecker>(
      script.url, script.resource_id, this);
  running_checker_->Start();
}

}  // namespace content
