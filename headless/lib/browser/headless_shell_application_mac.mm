// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_shell_application_mac.h"

#include "base/auto_reset.h"

@implementation HeadlessShellCrApplication

- (BOOL)isHandlingSendEvent {
  // The CrAppProtocol must return true if [NSApplication sendEvent:] is
  // currently on the stack. seee |CrAppProtocol| in |MessagePumpMac|.
  return true;
}

@end
