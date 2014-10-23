// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ACTIVITY_TYPE_IDS_H_
#define CHROME_BROWSER_ANDROID_ACTIVITY_TYPE_IDS_H_

// This file maps Activities on Chrome to specific flags for identification.

namespace ActivityTypeIds {

// Define Activities we are interested in tracking.  IDs are assigned
// consecutively, from NONE to MAX_VALUE.  Activities that are not explicitly
// defined are all assigned the UNKNOWN value.  When adding new ones, make sure
// to append them after current Activities and to update the |AndroidActivityId|
// enum in |histograms.xml|.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ActivityTypeIds
// GENERATED_JAVA_PREFIX_TO_STRIP: ACTIVITY_
enum Type {
  ACTIVITY_NONE = 0,
  ACTIVITY_UNKNOWN = 1,
  ACTIVITY_MAIN = 2,
  ACTIVITY_PREFERENCES = 3,
  ACTIVITY_WEBAPPACTIVITY = 4,
  ACTIVITY_FULLSCREENACTIVITY = 5,
  ACTIVITY_MAX_VALUE = 6
};

// Takes an int corresponding to a Type and returns the corresponding Type.
Type GetActivityType(int type_id);

}  // namespace ActivityTypeIds

#endif  // CHROME_BROWSER_ANDROID_ACTIVITY_TYPE_IDS_H_
