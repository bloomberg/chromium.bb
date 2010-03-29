// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enumerate the various item subtypes that are supported by sync.
// Each sync object is expected to have an immutable object type.
// An object's type is inferred from the type of data it holds.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_

#include "base/logging.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}

namespace syncable {

enum ModelType {
  UNSPECIFIED,       // Object type unknown.  Objects may transition through
                     // the unknown state during their initial creation, before
                     // their properties are set.  After deletion, object types
                     // are generally preserved.
  TOP_LEVEL_FOLDER,  // A permanent folder whose children may be of mixed
                     // datatypes (e.g. the "Google Chrome" folder).
  BOOKMARKS,         // A bookmark folder or a bookmark URL object.
  PREFERENCES,       // A preference folder or a preference object.
  AUTOFILL,          // An autofill folder or an autofill object.
  THEMES,            // A themes folder or a themes object.
  TYPED_URLS,        // A typed_url folder or a typed_url object.
  MODEL_TYPE_COUNT,
};

inline ModelType ModelTypeFromInt(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, MODEL_TYPE_COUNT);
  return static_cast<ModelType>(i);
}

void AddDefaultExtensionValue(syncable::ModelType datatype,
                              sync_pb::EntitySpecifics* specifics);

// Extract the model type of a SyncEntity protocol buffer.  ModelType is a
// local concept: the enum is not in the protocol.  The SyncEntity's ModelType
// is inferred from the presence of particular datatype extensions in the
// entity specifics.
ModelType GetModelType(const sync_pb::SyncEntity& sync_entity);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
