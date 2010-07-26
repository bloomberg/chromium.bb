// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_RLZ_H_
#define CHROME_BROWSER_RLZ_RLZ_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)

#include <string>

#include "base/basictypes.h"
#include "rlz/win/lib/rlz_lib.h"

// RLZ is a library which is used to measure distribution scenarios.
// Its job is to record certain lifetime events in the registry and to send
// them encoded as a compact string at most twice. The sent data does
// not contain information that can be used to identify a user or to infer
// browsing habits. The API in this file is a wrapper around the open source
// RLZ library which can be found at http://code.google.com/p/rlz.
//
// For partner or bundled installs, the RLZ might send more information
// according to the terms disclosed in the EULA.

class RLZTracker {

 public:
  // Like InitRlz() this function initializes the RLZ library services for use
  // in chrome. Besides binding the dll, it schedules a delayed task (delayed
  // by |delay| seconds) that performs the ping and registers some events
  // when 'first-run' is true.
  //
  // If the chrome brand is organic (no partners) then the RLZ library is not
  // loaded or initialized and the pings don't ocurr.
  static bool InitRlzDelayed(bool first_run, int delay);

  // Records an RLZ event. Some events can be access point independent.
  // Returns false it the event could not be recorded. Requires write access
  // to the HKCU registry hive on windows.
  static bool RecordProductEvent(rlz_lib::Product product,
                                 rlz_lib::AccessPoint point,
                                 rlz_lib::Event event_id);

  // Get the RLZ value of the access point.
  // Returns false if the rlz string could not be obtained. In some cases
  // an empty string can be returned which is not an error.
  static bool GetAccessPointRlz(rlz_lib::AccessPoint point, std::wstring* rlz);

  // Clear all events reported by this product. In Chrome this will be called
  // when it is un-installed.
  static bool ClearAllProductEvents(rlz_lib::Product product);

  // Invoked during shutdown to clean up any state created by RLZTracker.
  static void CleanupRlz();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RLZTracker);
};

#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_RLZ_RLZ_H_
