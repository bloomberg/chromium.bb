// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/activity_type_ids.h"

#include "base/logging.h"

namespace ActivityTypeIds {

Type GetActivityType(int type_id) {
  if (type_id >= ACTIVITY_NONE && type_id <= ACTIVITY_MAX_VALUE)
    return Type(type_id);

  NOTREACHED() << "Unhandled Activity id was passed in: " << type_id;
  return ACTIVITY_MAX_VALUE;
}

}  // namespace ActivityTypeIds
