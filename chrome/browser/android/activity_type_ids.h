// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ACTIVITY_TYPE_IDS_H_
#define CHROME_BROWSER_ANDROID_ACTIVITY_TYPE_IDS_H_

// This file maps Activities on Chrome to specific flags for identification.

namespace ActivityTypeIds {

enum Type {
#define DEFINE_ACTIVITY_ID(id,value) ACTIVITY_##id = (value),
#include "activity_type_id_list.h"
#undef DEFINE_ACTIVITY_ID
};  // enum Type

// Takes an int corresponding to a Type and returns the corresponding Type.
Type GetActivityType(int type_id);

}  // namespace ActivityTypeIds

#endif  // CHROME_BROWSER_ANDROID_ACTIVITY_TYPE_IDS_H_
