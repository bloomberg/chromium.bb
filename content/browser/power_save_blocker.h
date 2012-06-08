// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_SAVE_BLOCKER_H_
#define CONTENT_BROWSER_POWER_SAVE_BLOCKER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

// A RAII-style class to block the system from entering low-power (sleep) mode.
class CONTENT_EXPORT PowerSaveBlocker {
 public:
  enum PowerSaveBlockerType {
    kPowerSaveBlockPreventNone = -1,

    // Prevent the system from going to sleep; allow display sleep.
    kPowerSaveBlockPreventSystemSleep,

    // Prevent the system or display from going to sleep.
    kPowerSaveBlockPreventDisplaySleep,

    // Count of the values; not valid as a parameter.
    kPowerSaveBlockPreventStateCount
  };

  // Pass in the level of sleep prevention desired. kPowerSaveBlockPreventNone
  // is not a valid option.
  explicit PowerSaveBlocker(PowerSaveBlockerType type);
  ~PowerSaveBlocker();

 private:
  // Platform-specific function called when enable state is changed.
  // Guaranteed to be called only from the UI thread.
  static void ApplyBlock(PowerSaveBlockerType type);

  // Called only from UI thread.
  static void AdjustBlockCount(const std::vector<int>& deltas);

  // Invokes AdjustBlockCount on the UI thread.
  static void PostAdjustBlockCount(const std::vector<int>& deltas);

  // Returns the highest-severity block type in use.
  static PowerSaveBlockerType HighestBlockType();

  PowerSaveBlockerType type_;

  static int blocker_count_[];

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlocker);
};

namespace content {

// NOT READY YET. PowerSaveBlocker above is soon to be replaced by this class,
// but it's not done yet so client code should use the one above for now.
// A RAII-style class to block the system from entering low-power (sleep) mode.
// This class is thread-safe; it may be constructed and deleted on any thread.
class CONTENT_EXPORT PowerSaveBlocker2 {
 public:
  enum PowerSaveBlockerType {
    // Prevent the application from being suspended. On some platforms, apps may
    // be suspended when they are not visible to the user. This type of block
    // requests that the app continue to run in that case, and on all platforms
    // prevents the system from sleeping.
    // Example use cases: downloading a file, playing audio.
    kPowerSaveBlockPreventAppSuspension,

    // Prevent the display from going to sleep. This also has the side effect of
    // preventing the system from sleeping, but does not necessarily prevent the
    // app from being suspended on some platforms if the user hides it.
    // Example use case: playing video.
    kPowerSaveBlockPreventDisplaySleep,
  };

  // Pass in the type of power save blocking desired. If multiple types of
  // blocking are desired, instantiate one PowerSaveBlocker for each type.
  // |reason| may be provided to the underlying system APIs on some platforms.
  PowerSaveBlocker2(PowerSaveBlockerType type, const std::string& reason);
  ~PowerSaveBlocker2();

 private:
  class Delegate;

  // Implementations of this class may need a second object with different
  // lifetime than the RAII container, or additional storage. This member is
  // here for that purpose. If not used, just define the class as an empty
  // RefCounted (or RefCountedThreadSafe) like so to make it compile:
  // class PowerSaveBlocker2::Delegate
  //     : public base::RefCounted<PowerSaveBlocker2::Delegate> {
  //  private:
  //   friend class base::RefCounted<Delegate>;
  //   ~Delegate() {}
  // };
  scoped_refptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlocker2);
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_SAVE_BLOCKER_H_
