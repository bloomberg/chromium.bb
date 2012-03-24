// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_application_mac.h"

@implementation ShellCrApplication

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)sendEvent:(NSEvent*)event {
  BOOL wasHandlingSendEvent = handlingSendEvent_;
  handlingSendEvent_ = YES;
  [super sendEvent:event];
  handlingSendEvent_ = wasHandlingSendEvent;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

@end
