// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#import "chrome/browser/ui/cocoa/about_ipc_controller.h"
#include "content/public/browser/browser_ipc_logging.h"

#if defined(IPC_MESSAGE_LOG_ENABLED)

@implementation CocoaLogData

- (id)initWithLogData:(const IPC::LogData&)data {
  if ((self = [super init])) {
    data_ = data;
    // data_.message_name may not have been filled in if it originated
    // somewhere other than the browser process.
    if (data_.message_name == "")
      IPC::Logging::GetMessageText(data_.type, &data_.message_name, NULL, NULL);
  }
  return self;
}

- (NSString*)time {
  base::Time t = base::Time::FromInternalValue(data_.sent);
  base::Time::Exploded exploded;
  t.LocalExplode(&exploded);
  return [NSString stringWithFormat:@"%02d:%02d:%02d.%03d",
                   exploded.hour, exploded.minute,
                   exploded.second, exploded.millisecond];
}

- (NSString*)channel {
  return base::SysUTF8ToNSString(data_.channel);
}

- (NSString*)message {
  if (data_.message_name == "") {
    int high = data_.type >> 12;
    int low = data_.type - (high<<12);
    return [NSString stringWithFormat:@"type=(%d,%d) 0x%x,0x%x",
                     high, low, high, low];
  }
  else {
    return base::SysUTF8ToNSString(data_.message_name);
  }
}

- (NSString*)flags {
  return base::SysUTF8ToNSString(data_.flags);
}

- (NSString*)dispatch {
  base::Time sent = base::Time::FromInternalValue(data_.sent);
  int64 delta = (base::Time::FromInternalValue(data_.receive) -
                 sent).InMilliseconds();
  return [NSString stringWithFormat:@"%d", delta ? (int)delta : 0];
}

- (NSString*)process {
  base::TimeDelta delta = (base::Time::FromInternalValue(data_.dispatch) -
                           base::Time::FromInternalValue(data_.receive));
  int64 t = delta.InMilliseconds();
  return [NSString stringWithFormat:@"%d", t ? (int)t : 0];
}

- (NSString*)parameters {
  return base::SysUTF8ToNSString(data_.params);
}

@end

namespace {
AboutIPCController* gSharedController = nil;
}

@implementation AboutIPCController

+ (AboutIPCController*)sharedController {
  if (gSharedController == nil)
    gSharedController = [[AboutIPCController alloc] init];
  return gSharedController;
}

- (id)init {
  NSString* nibpath = [base::mac::FrameworkBundle() pathForResource:@"AboutIPC"
                                                             ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    // Default to all on
    appCache_ = view_ = utilityHost_ = viewHost_ = plugin_ =
      npObject_ = devTools_ = pluginProcessing_ = userString1_ =
      userString2_ = userString3_ = YES;
  }
  return self;
}

- (void)dealloc {
  if (gSharedController == self)
    gSharedController = nil;
  content::EnableIPCLogging(false);  // just in case...
  IPC::Logging::GetInstance()->SetConsumer(NULL);
  [super dealloc];
}

- (void)awakeFromNib {
  // Running Chrome with the --ipc-logging switch might cause it to
  // be enabled before the about:ipc window comes up; accomodate.
  [self updateVisibleRunState];

  // We are now able to display information, so let'er rip.
  bridge_.reset(new AboutIPCBridge(self));
  IPC::Logging::GetInstance()->SetConsumer(bridge_.get());
}

// Delegate callback.  Closing the window means there is no more need
// for the me, the controller.
- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

- (void)updateVisibleRunState {
  if (IPC::Logging::GetInstance()->Enabled())
    [startStopButton_ setTitle:@"Stop"];
  else
    [startStopButton_ setTitle:@"Start"];
}

- (IBAction)startStop:(id)sender {
  content::EnableIPCLogging(!IPC::Logging::GetInstance()->Enabled());
  [self updateVisibleRunState];
}

- (IBAction)clear:(id)sender {
  [dataController_ setContent:[NSMutableArray array]];
  [eventCount_ setStringValue:@"0"];
  [filteredEventCount_ setStringValue:@"0"];
  filteredEventCounter_ = 0;
}

// Return YES if we should filter this out; else NO.
// Just to be clear, [@"any string" hasPrefix:@""] returns NO.
- (BOOL)filterOut:(CocoaLogData*)data {
  NSString* name = [data message];
  if ((appCache_) && [name hasPrefix:@"AppCache"])
    return NO;
  if ((view_) && [name hasPrefix:@"ViewMsg"])
    return NO;
  if ((utilityHost_) && [name hasPrefix:@"UtilityHost"])
    return NO;
  if ((viewHost_) && [name hasPrefix:@"ViewHost"])
    return NO;
  if ((plugin_) && [name hasPrefix:@"PluginMsg"])
    return NO;
  if ((npObject_) && [name hasPrefix:@"NPObject"])
    return NO;
  if ((devTools_) && [name hasPrefix:@"DevTools"])
    return NO;
  if ((pluginProcessing_) && [name hasPrefix:@"PluginProcessing"])
    return NO;
  if ((userString1_) && ([name hasPrefix:[userStringTextField1_ stringValue]]))
    return NO;
  if ((userString2_) && ([name hasPrefix:[userStringTextField2_ stringValue]]))
    return NO;
  if ((userString3_) && ([name hasPrefix:[userStringTextField3_ stringValue]]))
    return NO;

  // Special case the unknown type.
  if ([name hasPrefix:@"type="])
    return NO;

  return YES;  // filter out.
}

- (void)log:(CocoaLogData*)data {
  if ([self filterOut:data]) {
    [filteredEventCount_ setStringValue:[NSString stringWithFormat:@"%d",
                                                  ++filteredEventCounter_]];
    return;
  }
  [dataController_ addObject:data];
  NSUInteger count = [[dataController_ arrangedObjects] count];
  // Uncomment if you want scroll-to-end behavior... but seems expensive.
  // [tableView_ scrollRowToVisible:count-1];
  [eventCount_ setStringValue:[NSString stringWithFormat:@"%ld",
                               static_cast<long>(count)]];
}

- (void)setDisplayViewMessages:(BOOL)display {
  view_ = display;
}

@end

#endif  // IPC_MESSAGE_LOG_ENABLED

