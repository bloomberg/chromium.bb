// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/external_data_fetcher.h"

#include "base/callback.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/external_data_manager.h"
#else
#include "chrome/browser/policy/stub_external_data_manager.h"
#endif

namespace policy {

ExternalDataFetcher::ExternalDataFetcher(const ExternalDataFetcher& other)
    : manager_(other.manager_),
      policy_(other.policy_) {
}

ExternalDataFetcher::~ExternalDataFetcher() {
}

// static
bool ExternalDataFetcher::Equals(const ExternalDataFetcher* first,
                                 const ExternalDataFetcher* second) {
  if (!first && !second)
    return true;
  if (!first || !second)
    return false;
  return first->manager_.get() == second->manager_.get() &&
         first->policy_ == second->policy_;
}

void ExternalDataFetcher::Fetch(const FetchCallback& callback) const {
  if (manager_)
    manager_->Fetch(policy_, callback);
  else
    callback.Run(STATUS_NO_DATA, make_scoped_ptr(new std::string));
}

ExternalDataFetcher::ExternalDataFetcher(
    base::WeakPtr<ExternalDataManager> manager,
    const std::string& policy)
    : manager_(manager),
      policy_(policy) {
}

}  // namespace policy
