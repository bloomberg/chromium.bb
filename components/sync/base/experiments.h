// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_EXPERIMENTS_H_
#define COMPONENTS_SYNC_BASE_EXPERIMENTS_H_

#include <string>

#include "components/sync/base/model_type.h"

namespace syncer {

const char kFaviconSyncTag[] = "favicon_sync";
const char kPreCommitUpdateAvoidanceTag[] = "pre_commit_update_avoidance";

// A structure to hold the enable status of experimental sync features.
struct Experiments {
  Experiments() : favicon_sync_limit(200) {}

  bool Matches(const Experiments& rhs) {
    return favicon_sync_limit == rhs.favicon_sync_limit;
  }

  // The number of favicons that a client is permitted to sync.
  int favicon_sync_limit;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_EXPERIMENTS_H_
