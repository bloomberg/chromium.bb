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
      LOG(WARNING) << "Entry does not belong to active ModelSafeGroup!";
    return GROUP_PASSIVE;
  }
  return it->second;
}

std::string ModelSafeGroupToString(ModelSafeGroup group) {
  switch (group) {
    case GROUP_UI:
      return "GROUP_UI";
    case GROUP_DB:
      return "GROUP_DB";
    case GROUP_HISTORY:
      return "GROUP_HISTORY";
    case GROUP_PASSIVE:
      return "GROUP_PASSIVE";
    case GROUP_PASSWORD:
      return "GROUP_PASSWORD";
    default:
      NOTREACHED();
      return "INVALID";
  }
}

ModelSafeWorker::ModelSafeWorker() {}

ModelSafeWorker::~ModelSafeWorker() {}

void ModelSafeWorker::DoWorkAndWaitUntilDone(Callback0::Type* work) {
  work->Run();  // For GROUP_PASSIVE, we do the work on the current thread.
}

ModelSafeGroup ModelSafeWorker::GetModelSafeGroup() {
  return GROUP_PASSIVE;
}

bool ModelSafeWorker::CurrentThreadIsWorkThread() {
  // The passive group is not the work thread for any browser model.
  return false;
}

}  // namespace browser_sync
