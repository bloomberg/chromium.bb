// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_

#include "base/logging.h"

namespace syncable {

enum ModelType {
  UNSPECIFIED = 0,
  BOOKMARKS,
  MODEL_TYPE_COUNT
};

inline ModelType ModelTypeFromInt(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, MODEL_TYPE_COUNT);
  return static_cast<ModelType>(i);
}

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_MODEL_TYPE_H_

