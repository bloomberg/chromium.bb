// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"

#include <objc/runtime.h>
#include <unordered_map>
#include <vector>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CommandDispatcher {
  // Stores which target to forward to for a given selector.
  std::unordered_map<SEL, __weak id> _forwardingTargets;
}

- (void)startDispatchingToTarget:(id)target forSelector:(SEL)selector {
  DCHECK(_forwardingTargets.find(selector) == _forwardingTargets.end());

  _forwardingTargets[selector] = target;
}

- (void)startDispatchingToTarget:(id)target forProtocol:(Protocol*)protocol {
  unsigned int methodCount;
  objc_method_description* requiredInstanceMethods =
      protocol_copyMethodDescriptionList(protocol, YES /* isRequiredMethod */,
                                         YES /* isInstanceMethod */,
                                         &methodCount);
  for (unsigned int i = 0; i < methodCount; i++) {
    [self startDispatchingToTarget:target
                       forSelector:requiredInstanceMethods[i].name];
  }
}

- (void)stopDispatchingForSelector:(SEL)selector {
  _forwardingTargets.erase(selector);
}

- (void)stopDispatchingForProtocol:(Protocol*)protocol {
  unsigned int methodCount;
  objc_method_description* requiredInstanceMethods =
      protocol_copyMethodDescriptionList(protocol, YES /* isRequiredMethod */,
                                         YES /* isInstanceMethod */,
                                         &methodCount);
  for (unsigned int i = 0; i < methodCount; i++) {
    [self stopDispatchingForSelector:requiredInstanceMethods[i].name];
  }
}

// |-stopDispatchingToTarget| should be called much less often than
// |-forwardingTargetForSelector|, so removal is intentionally O(n) in order
// to prioritize the speed of lookups.
- (void)stopDispatchingToTarget:(id)target {
  std::vector<SEL> selectorsToErase;
  for (auto& kv : _forwardingTargets) {
    if (kv.second == target) {
      selectorsToErase.push_back(kv.first);
    }
  }

  for (auto* selector : selectorsToErase) {
    [self stopDispatchingForSelector:selector];
  }
}

#pragma mark - NSObject

// Overridden to forward messages to registered handlers.
- (id)forwardingTargetForSelector:(SEL)selector {
  auto target = _forwardingTargets.find(selector);
  if (target != _forwardingTargets.end()) {
    return target->second;
  }
  return [super forwardingTargetForSelector:selector];
}

@end
