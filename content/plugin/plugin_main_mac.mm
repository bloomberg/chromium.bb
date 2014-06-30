// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "content/plugin/plugin_interpose_util_mac.h"

namespace content {

void InitializeChromeApplication() {
  [NSApplication sharedApplication];
  mac_plugin_interposing::SetUpCocoaInterposing();
}

}  // namespace content
