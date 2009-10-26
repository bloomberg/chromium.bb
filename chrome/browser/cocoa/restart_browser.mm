// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#import "chrome/browser/cocoa/restart_browser.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/app_strings.h"

// Helper to clean up after the notification that the alert was dismissed.
@interface RestartHelper : NSObject {
 @private
  NSAlert* alert_;
}
- (NSAlert*)alert;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

@implementation RestartHelper

- (NSAlert*)alert {
  alert_ = [[NSAlert alloc] init];
  return alert_;
}

- (void)dealloc {
  [alert_ release];
  [super dealloc];
}

- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  // Nothing to do, just clean up
  [self autorelease];
}

@end

namespace restart_browser {

void RequestRestart(NSWindow* parent) {
  NSString* title =
      l10n_util::GetNSStringFWithFixup(IDS_PLEASE_RESTART_BROWSER,
                                       l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_OPTIONS_RESTART_REQUIRED);
  NSString* okBtn = l10n_util::GetNSStringWithFixup(IDS_APP_OK);

  RestartHelper* helper = [[RestartHelper alloc] init];

  NSAlert* alert = [helper alert];
  [alert setAlertStyle:NSInformationalAlertStyle];
  [alert setMessageText:title];
  [alert setInformativeText:text];
  [alert addButtonWithTitle:okBtn];

  [alert beginSheetModalForWindow:parent
                    modalDelegate:helper
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                      contextInfo:nil];
}

}  // namespace restart_browser
