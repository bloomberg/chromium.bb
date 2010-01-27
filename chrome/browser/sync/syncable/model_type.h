// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enumerate the various item subtypes that are supported by sync.
// Each sync object is expected to have an immutable object type.
// An object's type is inferred from the type of data it holds.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_

#include "base/logging.h"

namespace syncable {

enum ModelType {
  UNSPECIFIED,       // Object type unknown.  Objects may transition through
                     // the unknown state during their initial creation, before
                     // their properties are set.  After deletion, object types
                     // are generally preserved.
  TOP_LEVEL_FOLDER,  // A permanent folder whose children may be of mixed
                     // datatypes (e.g. the "Google Chrome" folder).
  BOOKMARKS,         // A bookmark folder or a bookmark URL object.
  MODEL_TYPE_COUNT,
};

inline ModelType ModelTypeFromInt(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, MODEL_TYPE_COUNT);
  return static_cast<ModelType>(i);
}

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
