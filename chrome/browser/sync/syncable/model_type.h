// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enumerate the various item subtypes that are supported by sync.
// Each sync object is expected to have an immutable object type.
// An object's type is inferred from the type of data it holds.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
#pragma once

#include <bitset>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/time.h"

class ListValue;

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
  // A password folder or password object.
  PASSWORDS,
    // An AutofillProfile Object
  AUTOFILL_PROFILE,
  // An autofill folder or an autofill object.
  AUTOFILL,

  // A themes folder or a themes object.
  THEMES,
  // A typed_url folder or a typed_url object.
  TYPED_URLS,
  // An extension folder or an extension object.
  EXTENSIONS,
  // An object represeting a set of Nigori keys.
  NIGORI,
  // An object representing a browser session.
  SESSIONS,
  // An app folder or an app object.
  APPS,

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

// Determine a model type from the field number of its associated
// EntitySpecifics extension.
ModelType GetModelTypeFromExtensionFieldNumber(int field_number);

// Return the field number of the EntitySpecifics extension associated with
// a model type.
int GetExtensionFieldNumberFromModelType(ModelType model_type);

// Returns a string that represents the name of |model_type|.
std::string ModelTypeToString(ModelType model_type);

// Returns the ModelType corresponding to the name |model_type_string|.
ModelType ModelTypeFromString(const std::string& model_type_string);

// Converts a string into a model type bitset. If successful, returns true. If
// failed to parse string, returns false and model_types is unspecified.
bool ModelTypeBitSetFromString(
    const std::string& model_type_bitset_string,
    ModelTypeBitSet* model_types);

// Caller takes ownership of returned list.
ListValue* ModelTypeBitSetToValue(const ModelTypeBitSet& model_types);

// Caller takes ownership of returned list.
ListValue* ModelTypeSetToValue(const ModelTypeSet& model_types);

// Returns a string corresponding to the syncable tag for this datatype.
std::string ModelTypeToRootTag(ModelType type);

// Posts timedeltas to histogram of datatypes. Allows tracking of the frequency
// at which datatypes cause syncs.
void PostTimeToTypeHistogram(ModelType model_type, base::TimeDelta time);

// Convert a real model type to a notification type (used for
// subscribing to server-issued notifications).  Returns true iff
// |model_type| was a real model type and |notification_type| was
// filled in.
bool RealModelTypeToNotificationType(ModelType model_type,
                                     std::string* notification_type);

// Converts a notification type to a real model type.  Returns true
// iff |notification_type| was the notification type of a real model
// type and |model_type| was filled in.
bool NotificationTypeToRealModelType(const std::string& notification_type,
                                     ModelType* model_type);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
