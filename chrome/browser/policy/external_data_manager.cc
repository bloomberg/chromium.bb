// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/external_data_manager.h"

#include "base/callback.h"

namespace policy {

void ExternalDataManager::Fetch(
    const std::string& policy,
    const ExternalDataFetcher::FetchCallback& callback) {
  // TODO(bartfab): Implement support for fetching external policy data.
  // crbug.com/256635
}

}  // policy
