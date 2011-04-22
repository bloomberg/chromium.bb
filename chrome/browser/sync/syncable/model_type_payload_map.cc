// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type_payload_map.h"

#include "base/values.h"

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

}  // namespace syncable
