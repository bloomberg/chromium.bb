// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/restart_browser.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "grit/app_strings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

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
  if (returnCode == NSAlertFirstButtonReturn) {
    // Nothing to do. User will restart later.
  } else if (returnCode == NSAlertSecondButtonReturn) {
    // Set the flag to restore state after the restart.
    PrefService* pref_service = g_browser_process->local_state();
    pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
    BrowserList::CloseAllBrowsersAndExit();
  } else {
    NOTREACHED();
  }
  [self autorelease];
}

@end

namespace restart_browser {

void RequestRestart(NSWindow* parent) {
  NSString* title =
      l10n_util::GetNSStringFWithFixup(IDS_PLEASE_RELAUNCH_BROWSER,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* text =
      l10n_util::GetNSStringFWithFixup(IDS_UPDATE_RECOMMENDED,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  NSString* notNowButtin = l10n_util::GetNSStringWithFixup(IDS_NOT_NOW);
  NSString* restartButton =
      l10n_util::GetNSStringWithFixup(IDS_RELAUNCH_AND_UPDATE);

  RestartHelper* helper = [[RestartHelper alloc] init];

  NSAlert* alert = [helper alert];
  [alert setAlertStyle:NSInformationalAlertStyle];
  [alert setMessageText:title];
  [alert setInformativeText:text];
  [alert addButtonWithTitle:notNowButtin];
  [alert addButtonWithTitle:restartButton];

  if (parent) {
    [alert beginSheetModalForWindow:parent
                      modalDelegate:helper
                     didEndSelector:@selector(alertDidEnd:
                                               returnCode:
                                              contextInfo:)
                        contextInfo:nil];
  } else {
    NSInteger returnCode = [alert runModal];
    [helper alertDidEnd:alert
             returnCode:returnCode
            contextInfo:NULL];
  }
}

}  // namespace restart_browser
