// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/browser/pref_member.h"

namespace ContentSettingsDialogControllerInternal {
class PrefObserverBridge;
}

class Profile;

// This controller manages a dialog that lets the user manage the content
// settings for several content setting types.
@interface ContentSettingsDialogController
    : NSWindowController<NSWindowDelegate, NSTabViewDelegate> {
 @private
  IBOutlet NSTabView* tabView_;
  Profile* profile_;  // weak
  IntegerPrefMember lastSelectedTab_;
  BooleanPrefMember clearSiteDataOnExit_;
  scoped_ptr<ContentSettingsDialogControllerInternal::PrefObserverBridge>
      observer_;  // Watches for pref changes.
}

// Show the content settings dialog associated with the given profile (or the
// original profile if this is an incognito profile).  If no content settings
// dialog exists for this profile, create one and show it.  Any resulting
// editor releases itself when closed.
+(id)showContentSettingsForType:(ContentSettingsType)settingsType
                        profile:(Profile*)profile;

- (IBAction)showCookies:(id)sender;
- (IBAction)openFlashPlayerSettings:(id)sender;

- (IBAction)showCookieExceptions:(id)sender;
- (IBAction)showImagesExceptions:(id)sender;
- (IBAction)showJavaScriptExceptions:(id)sender;
- (IBAction)showPluginsExceptions:(id)sender;
- (IBAction)showPopupsExceptions:(id)sender;

@end
