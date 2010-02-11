// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

ModelSafeGroup GetGroupForEntry(const syncable::Entry* e,
                                const ModelSafeRoutingInfo& routes) {
  ModelSafeRoutingInfo::const_iterator it = routes.find(e->GetModelType());
  if (it == routes.end()) {
    NOTREACHED() << "Entry does not belong to active ModelSafeGroup!";
    return GROUP_PASSIVE;
  }
  return it->second;
}

}  // namespace browser_sync
