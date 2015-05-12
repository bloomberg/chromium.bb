// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mac/mac_init.h"

#include <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"

namespace content {

void InitializeMac() {
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
      @"NSTreatUnknownArgumentsAsOpen": @"NO",
      // CoreAnimation has poor performance and CoreAnimation and
      // non-CoreAnimation exhibit window flickering when layers are not hosted
      // in the window server, which is the default when not not using the
      // 10.9 SDK.
      // TODO: Remove this when we build with the 10.9 SDK.
      @"NSWindowHostsLayersInWindowServer": @(base::mac::IsOSMavericksOrLater())
  }];
}

}  // namespace content
