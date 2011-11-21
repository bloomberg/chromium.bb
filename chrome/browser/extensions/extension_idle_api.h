// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_H_
#pragma once

#include "chrome/browser/idle.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

// Event router class for events related to the idle API.
class ExtensionIdleEventRouter {
 public:
  static void OnIdleStateChange(Profile* profile,
                                IdleState idleState);
 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionIdleEventRouter);
};

// Implementation of the chrome.idle.queryState API.
class ExtensionIdleQueryStateFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("idle.queryState")

 private:
  void IdleStateCallback(int threshold, IdleState state);
};

// Class used for caching answers from CalculateIdleState.
class ExtensionIdleCache {
 public:
  static IdleState CalculateIdleState(int threshold);
  static void UpdateCache(int threshold, IdleState state);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionIdleApiTest, CacheTest);

  struct CacheData {
    // Latest moment in history after which we are certain that there was some
    // activity. If we are querying for a  threshold beyond this moment, we know
    // the result will be active.
    double latest_known_active;
    // [idle_interval_start, idle_interval_end] is the latest interval in
    // history for which we are certain there was no activity.
    double idle_interval_start;
    double idle_interval_end;
    // Set iff the last recored query result was IDLE_STATE_LOCKED. Equals the
    // moment the result was recorded in cache.
    double latest_locked;
  };

  // We assume moment is increasing with every call to one of these two methods.

  // Tries to determine the idle state based on results of previous queries.
  // |threshold| is time span in seconds from now we consider in calculating
  // state.
  // |moment| is the moment in time this method was called (should be equal to
  // now, except in tests).
  // Returns calculated state, or IDLE_STATE_UNKNOWN if the state could not be
  // determined.
  static IdleState CalculateState(int threshold, double moment);

  // Updates cached data with the latest query result.
  // |threshold| is threshold parameter of the query.
  // |state| is result of the query.
  // |moment| is moment in time this method is called.
  static void Update(int threshold, IdleState state, double moment);

  static int get_min_threshold();
  static double get_throttle_interval();

  static CacheData cached_data;
};
#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_H_
