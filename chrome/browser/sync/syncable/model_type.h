// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enumerate the various item subtypes that are supported by sync.
// Each sync object is expected to have an immutable object type.
// An object's type is inferred from the type of data it holds.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_

#include <bitset>
#include <set>

#include "base/logging.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}

namespace syncable {

enum ModelType {
  // Object type unknown.  Objects may transition through
  // the unknown state during their initial creation, before
  // their properties are set.  After deletion, object types
  // are generally preserved.
  UNSPECIFIED,
  // A permanent folder whose children may be of mixed
  // datatypes (e.g. the "Google Chrome" folder).
  TOP_LEVEL_FOLDER,

  // ------------------------------------ Start of "real" model types.
  // The model types declared before here are somewhat special, as they
  // they do not correspond to any browser data model.  The remaining types
  // are bona fide model types; all have a related browser data model and
  // can be represented in the protocol using an extension to the
  // EntitySpecifics protocol buffer.
  //
  // A bookmark folder or a bookmark URL object.
  BOOKMARKS,
  FIRST_REAL_MODEL_TYPE = BOOKMARKS,  // Declared 2nd, for debugger prettiness.

  // A preference folder or a preference object.
  PREFERENCES,
  // An autofill folder or an autofill object.
  AUTOFILL,
  // A themes folder or a themes object.
  THEMES,
  // A typed_url folder or a typed_url object.
  TYPED_URLS,

  MODEL_TYPE_COUNT,
};

typedef std::bitset<MODEL_TYPE_COUNT> ModelTypeBitSet;
typedef std::set<ModelType> ModelTypeSet;

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

// Extract the model type from an EntitySpecifics extension.  Note that there
// are some ModelTypes (like TOP_LEVEL_FOLDER) that can't be inferred this way;
// prefer using GetModelType where possible.
ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
