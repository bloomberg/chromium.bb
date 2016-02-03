// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/memory/memory_debugger_manager.h"

#include "base/ios/weak_nsobject.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/memory/memory_debugger.h"
#import "ios/chrome/browser/pref_names.h"

@implementation MemoryDebuggerManager {
  UIView* debuggerParentView_;  // weak
  base::scoped_nsobject<MemoryDebugger> memoryDebugger_;
  BooleanPrefMember showMemoryDebugger_;
}

- (instancetype)initWithView:(UIView*)debuggerParentView
                       prefs:(PrefService*)prefs {
  if (self = [super init]) {
    debuggerParentView_ = debuggerParentView;

    // Set up the callback for when the pref to show/hide the debugger changes.
    base::WeakNSObject<MemoryDebuggerManager> weakSelf(self);
    base::Closure callback = base::BindBlock(^{
      base::scoped_nsobject<MemoryDebuggerManager> strongSelf(
          [weakSelf retain]);
      if (strongSelf) {
        [self onShowMemoryDebuggingToolsChange];
      }
    });
    showMemoryDebugger_.Init(prefs::kShowMemoryDebuggingTools, prefs, callback);
    // Invoke the pref change callback once to show the debugger on start up,
    // if necessary.
    [self onShowMemoryDebuggingToolsChange];
  }
  return self;
}

- (void)dealloc {
  [self tearDownDebugger];
  [super dealloc];
}

#pragma mark - Pref-handling methods

// Registers local state prefs.
+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(prefs::kShowMemoryDebuggingTools, false);
}

// Shows or hides the debugger when the pref changes.
- (void)onShowMemoryDebuggingToolsChange {
  if (showMemoryDebugger_.GetValue()) {
    memoryDebugger_.reset([[MemoryDebugger alloc] init]);
    [debuggerParentView_ addSubview:memoryDebugger_];
  } else {
    [self tearDownDebugger];
  }
}

// Tears down the debugger so it can be deallocated.
- (void)tearDownDebugger {
  [memoryDebugger_ invalidateTimers];
  [memoryDebugger_ removeFromSuperview];
  memoryDebugger_.reset();
}
@end
