// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_MAC_SCOPED_NSDISABLE_SCREEN_UPDATES_H_
#define APP_MAC_SCOPED_NSDISABLE_SCREEN_UPDATES_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"

namespace app {
namespace mac {

// A stack-based class to disable Cocoa screen updates. When instantiated, it
// disables screen updates and enables them when destroyed. Update disabling
// can be nested, and there is a time-maximum (about 1 second) after which
// Cocoa will automatically re-enable updating. This class doesn't attempt to
// overrule that.
class ScopedNSDisableScreenUpdates {
 public:
  ScopedNSDisableScreenUpdates() {
    NSDisableScreenUpdates();
  }
  ~ScopedNSDisableScreenUpdates() {
    NSEnableScreenUpdates();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedNSDisableScreenUpdates);
};

}  // namespace mac
}  // namespace app

#endif  // APP_MAC_SCOPED_NSDISABLE_SCREEN_UPDATES_H_
