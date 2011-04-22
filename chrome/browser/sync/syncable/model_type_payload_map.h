// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Definition of ModelTypePayloadMap and various utility functions.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_PAYLOAD_MAP_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_PAYLOAD_MAP_H_
#pragma once

#include <map>
#include <string>

#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"

class DictionaryValue;

namespace syncable {

// A container that contains a set of datatypes with possible string
// payloads.
typedef std::map<ModelType, std::string> ModelTypePayloadMap;

// Helper functions for building ModelTypePayloadMaps.

// Make a TypePayloadMap from all the types in a ModelTypeBitSet using
// a default payload.
ModelTypePayloadMap ModelTypePayloadMapFromBitSet(
    const ModelTypeBitSet& model_types,
    const std::string& payload);

// Make a TypePayloadMap for all the enabled types in a
// ModelSafeRoutingInfo using a default payload.
ModelTypePayloadMap ModelTypePayloadMapFromRoutingInfo(
    const browser_sync::ModelSafeRoutingInfo& routes,
    const std::string& payload);

// Caller takes ownership of the returned dictionary.
DictionaryValue* ModelTypePayloadMapToValue(
    const ModelTypePayloadMap& model_type_payloads);

// Coalesce |update| into |original|, overwriting only when |update| has
// a non-empty payload.
void CoalescePayloads(ModelTypePayloadMap* original,
                      const ModelTypePayloadMap& update);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_PAYLOAD_MAP_H_
