// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/browser/pref_member.h"

class Profile;

// This controller manages a dialog that lets the user manage the content
// settings for several content setting types.
@interface ContentSettingsDialogController
    : NSWindowController<NSWindowDelegate> {
 @private
  Profile* profile_;  // weak
  IntegerPrefMember lastSelectedTab_;
  BooleanPrefMember clearSiteDataOnExit_;
}

// Show the content settings dialog associated with the given profile (or the
// original profile if this is an incognito profile).  If no content settings
// dialog exists for this profile, create one and show it.  Any resulting
// editor releases itself when closed.
+(id)showContentSettingsForType:(ContentSettingsType)settingsType
                        profile:(Profile*)profile;

- (IBAction)showCookies:(id)sender;
- (IBAction)openFlashPlayerSettings:(id)sender;

@end
