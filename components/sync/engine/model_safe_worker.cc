// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_safe_worker.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace syncer {

std::unique_ptr<base::DictionaryValue> ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    dict->SetString(ModelTypeToString(it->first),
                    ModelSafeGroupToString(it->second));
  }
  return dict;
}

std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info) {
  std::string json;
  base::JSONWriter::Write(*ModelSafeRoutingInfoToValue(routing_info), &json);
  return json;
}

ModelTypeSet GetRoutingInfoTypes(const ModelSafeRoutingInfo& routing_info) {
  ModelTypeSet types;
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    types.Put(it->first);
  }
  return types;
}

ModelSafeGroup GetGroupForModelType(const ModelType type,
                                    const ModelSafeRoutingInfo& routes) {
  ModelSafeRoutingInfo::const_iterator it = routes.find(type);
  if (it == routes.end()) {
    if (type != UNSPECIFIED && type != TOP_LEVEL_FOLDER)
      DVLOG(1) << "Entry does not belong to active ModelSafeGroup!";
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
    case GROUP_FILE:
      return "GROUP_FILE";
    case GROUP_HISTORY:
      return "GROUP_HISTORY";
    case GROUP_PASSIVE:
      return "GROUP_PASSIVE";
    case GROUP_PASSWORD:
      return "GROUP_PASSWORD";
    case GROUP_NON_BLOCKING:
      return "GROUP_NON_BLOCKING";
    default:
      NOTREACHED();
      return "INVALID";
  }
}

ModelSafeWorker::ModelSafeWorker() {}
ModelSafeWorker::~ModelSafeWorker() {}

void ModelSafeWorker::RequestStop() {
  // Set stop flag. This prevents any *further* tasks from being posted to
  // worker threads (see DoWorkAndWaitUntilDone below), but note that one may
  // already be posted.
  stopped_.Set();
}

SyncerError ModelSafeWorker::DoWorkAndWaitUntilDone(const WorkCallback& work) {
  if (stopped_.IsSet())
    return CANNOT_DO_WORK;
  return DoWorkAndWaitUntilDoneImpl(work);
}

bool ModelSafeWorker::IsStopped() {
  return stopped_.IsSet();
}

}  // namespace syncer
