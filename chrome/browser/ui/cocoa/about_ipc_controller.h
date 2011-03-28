// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ABOUT_IPC_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_ABOUT_IPC_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/GTM/Foundation/GTMRegex.h"

// Must be included after IPC_MESSAGE_LOG_ENABLED gets defined
#import "chrome/browser/ui/cocoa/about_ipc_dialog.h"

#if defined(IPC_MESSAGE_LOG_ENABLED)

// An objc wrapper for IPC::LogData to allow use of Cocoa bindings.
@interface CocoaLogData : NSObject {
 @private
  IPC::LogData data_;
}
- (id)initWithLogData:(const IPC::LogData&)data;
@end


// A window controller that handles the about:ipc non-modal dialog.
@interface AboutIPCController : NSWindowController {
 @private
  scoped_ptr<AboutIPCBridge> bridge_;
  IBOutlet NSButton* startStopButton_;
  IBOutlet NSTableView* tableView_;
  IBOutlet NSArrayController* dataController_;
  IBOutlet NSTextField* eventCount_;
  IBOutlet NSTextField* filteredEventCount_;
  IBOutlet NSTextField* userStringTextField1_;
  IBOutlet NSTextField* userStringTextField2_;
  IBOutlet NSTextField* userStringTextField3_;
  // Count of filtered events.
  int filteredEventCounter_;
  // Cocoa-bound to check boxes for filtering messages.
  // Each BOOL allows events that have that name prefix.
  // E.g. if set, appCache_ allows events named AppCache*.
  // The actual string to match is defined in the xib.
  // The userStrings allow a user-specified prefix.
  BOOL appCache_;
  BOOL view_;
  BOOL utilityHost_;
  BOOL viewHost_;
  BOOL plugin_;
  BOOL npObject_;
  BOOL devTools_;
  BOOL pluginProcessing_;
  BOOL userString1_;
  BOOL userString2_;
  BOOL userString3_;
}

+ (AboutIPCController*)sharedController;

- (IBAction)startStop:(id)sender;
- (IBAction)clear:(id)sender;

// Called from our C++ bridge class.  To accomodate multithreaded
// ownership issues, this method ACCEPTS OWNERSHIP of the arg passed
// in.
- (void)log:(CocoaLogData*)data;

// Update visible state (e.g. Start/Stop button) based on logging run
// state.  Does not change state.
- (void)updateVisibleRunState;

@end

@interface AboutIPCController(TestingAPI)
- (BOOL)filterOut:(CocoaLogData*)data;
- (void)setDisplayViewMessages:(BOOL)display;
@end

#endif  // IPC_MESSAGE_LOG_ENABLED
#endif  // CHROME_BROWSER_UI_COCOA_ABOUT_IPC_CONTROLLER_H_
