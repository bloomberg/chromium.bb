// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/util/unrecoverable_error_info.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace browser_sync {

base::DictionaryValue* ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    dict->SetString(syncable::ModelTypeToString(it->first),
                    ModelSafeGroupToString(it->second));
  }
  return dict;
}

std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info) {
  scoped_ptr<DictionaryValue> dict(ModelSafeRoutingInfoToValue(routing_info));
  std::string json;
  base::JSONWriter::Write(dict.get(), false, &json);
  return json;
}

syncable::ModelTypeSet GetRoutingInfoTypes(
    const ModelSafeRoutingInfo& routing_info) {
  syncable::ModelTypeSet types;
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    types.insert(it->first);
  }
  return types;
}

ModelSafeGroup GetGroupForModelType(const syncable::ModelType type,
                                    const ModelSafeRoutingInfo& routes) {
  ModelSafeRoutingInfo::const_iterator it = routes.find(type);
  if (it == routes.end()) {
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
    case GROUP_FILE:
      return "GROUP_FILE";
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

ModelSafeWorker::~ModelSafeWorker() {}

}  // namespace browser_sync
