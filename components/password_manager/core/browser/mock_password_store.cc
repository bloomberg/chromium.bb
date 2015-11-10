// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_password_store.h"

namespace password_manager {

MockPasswordStore::MockPasswordStore()
    : PasswordStore(base::ThreadTaskRunnerHandle::Get(),
                    base::ThreadTaskRunnerHandle::Get()) {
}

MockPasswordStore::~MockPasswordStore() {
}

ScopedVector<InteractionsStats> MockPasswordStore::GetSiteStatsImpl(
    const GURL& origin_domain) {
  std::vector<InteractionsStats*> stats = GetSiteStatsMock(origin_domain);
  ScopedVector<InteractionsStats> result;
  result.swap(stats);
  return result.Pass();
}

}  // namespace password_manager
