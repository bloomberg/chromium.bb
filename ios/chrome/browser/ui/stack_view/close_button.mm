// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/stack_view/close_button.h"

#include "base/logging.h"

@implementation CloseButton {
  id _accessibilityTarget;  // weak
  SEL _accessibilityAction;
}

- (void)addAccessibilityElementFocusedTarget:(id)accessibilityTarget
                                      action:(SEL)accessibilityAction {
  DCHECK(!accessibilityTarget ||
         [accessibilityTarget respondsToSelector:accessibilityAction]);
  _accessibilityTarget = accessibilityTarget;
  _accessibilityAction = accessibilityAction;
}

- (void)accessibilityElementDidBecomeFocused {
  [_accessibilityTarget performSelector:_accessibilityAction withObject:self];
}

@end
