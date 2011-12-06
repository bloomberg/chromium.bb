// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_TRIAL_H_
#define CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_TRIAL_H_
#pragma once

// Strings used with default apps field trial.  If the field trial is running
// base::FieldTrialList::TrialExists(kDefaultAppsTrial_Name) returns true.
//
// The field trial consists of two groups of users: those that get default apps
// installed in the NTP and those that don't.  The group_name() of the field
// trial object is used to determine the group that the user belongs to.
//
// The field trial is setup in ChromeBrowserMainParts::DefaultAppsFieldTrial()
// based on the user's brand code:
//
//   - brand ECDA gets default apps
//   - brand ECDB does not get default apps
//   - any other brand code get default apps, but they are not part of the
//     trial

extern const char kDefaultAppsTrialName[];
extern const char kDefaultAppsTrialNoAppsGroup[];
extern const char kDefaultAppsTrialWithAppsGroup[];

#endif  // CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_TRIAL_H_
