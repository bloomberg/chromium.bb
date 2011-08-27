// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type_payload_map.h"

#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

using browser_sync::ModelSafeRoutingInfo;
namespace syncable {

ModelTypePayloadMap ModelTypePayloadMapFromBitSet(
    const syncable::ModelTypeBitSet& types,
    const std::string& payload) {
  ModelTypePayloadMap types_with_payloads;
  for (size_t i = syncable::FIRST_REAL_MODEL_TYPE;
       i < types.size(); ++i) {
    if (types[i]) {
      types_with_payloads[syncable::ModelTypeFromInt(i)] = payload;
    }
  }
  return types_with_payloads;
}

ModelTypeSet ModelTypePayloadMapToSet(const ModelTypePayloadMap& payload_map) {
  ModelTypeSet types;
  for (ModelTypePayloadMap::const_iterator it = payload_map.begin();
       it != payload_map.end(); ++it) {
    types.insert(it->first);
  }
  return types;
}

ModelTypePayloadMap ModelTypePayloadMapFromRoutingInfo(
    const browser_sync::ModelSafeRoutingInfo& routes,
    const std::string& payload) {
  ModelTypePayloadMap types_with_payloads;
  for (browser_sync::ModelSafeRoutingInfo::const_iterator i = routes.begin();
       i != routes.end(); ++i) {
    types_with_payloads[i->first] = payload;
  }
  return types_with_payloads;
}

std::string ModelTypePayloadMapToString(
    const ModelTypePayloadMap& type_payloads) {
  scoped_ptr<DictionaryValue> value(
      ModelTypePayloadMapToValue(type_payloads));
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  return json;
}

DictionaryValue* ModelTypePayloadMapToValue(
    const ModelTypePayloadMap& type_payloads) {
  DictionaryValue* value = new DictionaryValue();
  for (ModelTypePayloadMap::const_iterator it = type_payloads.begin();
       it != type_payloads.end(); ++it) {
    value->SetString(syncable::ModelTypeToString(it->first), it->second);
  }
  return value;
}

void CoalescePayloads(ModelTypePayloadMap* original,
                      const ModelTypePayloadMap& update) {
  for (ModelTypePayloadMap::const_iterator i = update.begin();
       i != update.end(); ++i) {
    if (original->count(i->first) == 0) {
      // If this datatype isn't already in our map, add it with
      // whatever payload it has.
      (*original)[i->first] = i->second;
    } else if (i->second.length() > 0) {
      // If this datatype is already in our map, we only overwrite the
      // payload if the new one is non-empty.
      (*original)[i->first] = i->second;
    }
  }
}

void PurgeStalePayload(ModelTypePayloadMap* original,
                       const ModelSafeRoutingInfo& routing_info) {
  std::vector<ModelTypePayloadMap::iterator> iterators_to_delete;
  for (ModelTypePayloadMap::iterator i = original->begin();
       i != original->end(); ++i) {
    if (routing_info.end() == routing_info.find(i->first)) {
      iterators_to_delete.push_back(i);
    }
  }

  for (std::vector<ModelTypePayloadMap::iterator>::iterator
       it = iterators_to_delete.begin(); it != iterators_to_delete.end();
       ++it) {
    original->erase(*it);
  }
}

}  // namespace syncable
