// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_safe_worker.h"

namespace browser_sync {

ModelSafeGroup GetGroupForModelType(const syncable::ModelType type,
                                    const ModelSafeRoutingInfo& routes) {
  ModelSafeRoutingInfo::const_iterator it = routes.find(type);
  if (it == routes.end()) {
    // TODO(tim): We shouldn't end up here for TOP_LEVEL_FOLDER, but an issue
    // with the server's PermanentItemPopulator is causing TLF updates in
    // some cases.  See bug 36735.
    if (type != syncable::UNSPECIFIED && type != syncable::TOP_LEVEL_FOLDER)
      NOTREACHED() << "Entry does not belong to active ModelSafeGroup!";
    return GROUP_PASSIVE;
  }
  return it->second;
}

}  // namespace browser_sync
