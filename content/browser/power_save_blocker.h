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

namespace content {

// A RAII-style class to block the system from entering low-power (sleep) mode.
// This class is thread-safe; it may be constructed and deleted on any thread.
class CONTENT_EXPORT PowerSaveBlocker {
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
  PowerSaveBlocker(PowerSaveBlockerType type, const std::string& reason);
  ~PowerSaveBlocker();

 private:
  class Delegate;

  // Implementations of this class may need a second object with different
  // lifetime than the RAII container, or additional storage. This member is
  // here for that purpose. If not used, just define the class as an empty
  // RefCounted (or RefCountedThreadSafe) like so to make it compile:
  // class PowerSaveBlocker::Delegate
  //     : public base::RefCounted<PowerSaveBlocker::Delegate> {
  //  private:
  //   friend class base::RefCounted<Delegate>;
  //   ~Delegate() {}
  // };
  scoped_refptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlocker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_SAVE_BLOCKER_H_
