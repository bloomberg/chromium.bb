// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NAVIGATION_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_NAVIGATION_TRACKER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/devtools_event_listener.h"

namespace base {
class DictionaryValue;
}

// Tracks the navigation state of frames.
class NavigationTracker : public DevToolsEventListener {
 public:
  NavigationTracker();
  virtual ~NavigationTracker();

  bool IsPendingNavigation(const std::string& frame_id);

  // Overridden from DevToolsEventListener:
  virtual void OnEvent(const std::string& method,
                       const base::DictionaryValue& params) OVERRIDE;

 private:
  class NavigationState {
   public:
    NavigationState();
    ~NavigationState();

    bool IsPendingNavigation();

    typedef unsigned int Mask;
    void SetFlags(Mask mask);
    void ClearFlags(Mask mask);

    enum Flags {
        LOADING = 0x1,
        SCHEDULED = 0x2,
    };

   private:
    Mask state_bitfield_;
  };

  std::map<std::string, NavigationState> frame_state_;

  DISALLOW_COPY_AND_ASSIGN(NavigationTracker);
};

#endif  // CHROME_TEST_CHROMEDRIVER_NAVIGATION_TRACKER_H_
