// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_settings_dialog_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/mac_util.h"
#import "chrome/browser/cocoa/cookies_window_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"


namespace {

// Stores the currently visible content settings dialog, if any.
ContentSettingsDialogController* g_instance = nil;

}  // namespace


@interface ContentSettingsDialogController(Private)
- (id)initWithProfile:(Profile*)profile;
@end


@implementation ContentSettingsDialogController

+(id)showContentSettingsForType:(ContentSettingsType)settingsType
                        profile:(Profile*)profile {
  profile = profile->GetOriginalProfile();
  if (!g_instance)
    g_instance = [[self alloc] initWithProfile:profile];

  // The code doesn't expect multiple profiles. Check that support for that
  // hasn't been added.
  DCHECK(g_instance->profile_ == profile);

  // Select desired tab.
  if (settingsType == CONTENT_SETTINGS_TYPE_DEFAULT) {
    // Remember the last visited page from local state.
    int value = g_instance->lastSelectedTab_.GetValue();
    if (value >= 0 && value < CONTENT_SETTINGS_NUM_TYPES)
      settingsType = static_cast<ContentSettingsType>(value);
    if (settingsType == CONTENT_SETTINGS_TYPE_DEFAULT)
      settingsType = CONTENT_SETTINGS_TYPE_COOKIES;
  }
  // TODO(thakis): Actually select desired tab.

  [g_instance showWindow:nil];
  return g_instance;
}

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"ContentSettings"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;

    // We don't need to observe changes in this value.
    lastSelectedTab_.Init(prefs::kContentSettingsWindowLastTabIndex,
                          profile->GetPrefs(), NULL);
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
  g_instance = nil;
}

// Shows the cookies controller.
- (IBAction)showCookies:(id)sender {
  // The cookie controller will autorelease itself when it's closed.
  BrowsingDataDatabaseHelper* databaseHelper =
      new BrowsingDataDatabaseHelper(profile_);
  BrowsingDataLocalStorageHelper* storageHelper =
      new BrowsingDataLocalStorageHelper(profile_);
  CookiesWindowController* controller =
      [[CookiesWindowController alloc] initWithProfile:profile_
                                        databaseHelper:databaseHelper
                                         storageHelper:storageHelper];
  [controller attachSheetTo:[self window]];
}

@end
