// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the iOS equivalent of FirstRun.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_

#include "base/macros.h"

namespace base {
class FilePath;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A collection of functions to manage the user experience when running
// this application for the first time. The iOS implementation is significantly
// simpler than other platforms because it is designed to be preemptive and
// stops user from doing anything else until the First Run UX is completed
// or explicitly skipped.
class FirstRun {
 public:
  // Returns true if this is the first time chrome is run for this user.
  static bool IsChromeFirstRun();

  // Creates the sentinel file that signals that chrome has been configured iff
  // the file does not exist yet. Returns true if the file was created and false
  // if the file already exists or could not be created.
  static bool CreateSentinel();

  // Removes the sentinel file created in ConfigDone(). Returns false if the
  // sentinel file could not be removed.
  static bool RemoveSentinel();

  // Get RLZ ping delay pref name.
  static const char* GetPingDelayPrefName();

  // Register user preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Gives the full path to the sentinel file. The file might not exist.
  static bool GetFirstRunSentinelFilePath(base::FilePath* path);

  enum FirstRunState {
    FIRST_RUN_UNKNOWN,  // The state is not tested or set yet.
    FIRST_RUN_TRUE,
    FIRST_RUN_FALSE
  };

  // This variable should only be accessed through IsChromeFirstRun().
  static FirstRunState first_run_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FirstRun);
};

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
