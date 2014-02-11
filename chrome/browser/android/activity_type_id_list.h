// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intentionally no include guards because this file is meant to be included
// inside a macro to generate enum values.

// Define Activities we are interested in tracking.  IDs are assigned
// consecutively, from NONE to MAX_VALUE.  Activities that are not explicitly
// defined are all assigned the UNKNOWN value.  When adding new ones, make sure
// to append them after current Activities and to update the |AndroidActivityId|
// enum in |histograms.xml|.
DEFINE_ACTIVITY_ID(NONE, 0)
DEFINE_ACTIVITY_ID(UNKNOWN, 1)
DEFINE_ACTIVITY_ID(MAIN, 2)
DEFINE_ACTIVITY_ID(PREFERENCES, 3)
DEFINE_ACTIVITY_ID(WEBAPPACTIVITY, 4)
DEFINE_ACTIVITY_ID(FULLSCREENACTIVITY, 5)
DEFINE_ACTIVITY_ID(MAX_VALUE, 6)
