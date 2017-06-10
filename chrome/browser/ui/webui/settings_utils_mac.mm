// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/webui/settings_utils.h"

#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_aedesc.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {
void ValidateFontFamily(PrefService* prefs, const char* family_pref_name) {
  // The native font settings dialog saved fonts by the font name, rather
  // than the family name.  This worked for the old dialog since
  // -[NSFont fontWithName:size] accepted a font or family name, but the
  // behavior was technically wrong.  Since we really need the family name for
  // the webui settings window, we will fix the saved preference if necessary.
  NSString* family_name =
      base::SysUTF8ToNSString(prefs->GetString(family_pref_name));
  NSFont* font = [NSFont fontWithName:family_name size:[NSFont systemFontSize]];
  if (font &&
      [[font familyName] caseInsensitiveCompare:family_name] != NSOrderedSame) {
    std::string new_family_name = base::SysNSStringToUTF8([font familyName]);
    prefs->SetString(family_pref_name, new_family_name);
  }
}
}  // namespace

namespace settings_utils {

void ShowNetworkProxySettings(content::WebContents* web_contents) {
  NSArray* itemsToOpen = [NSArray arrayWithObject:[NSURL fileURLWithPath:
      @"/System/Library/PreferencePanes/Network.prefPane"]];

  const char* proxyPrefCommand = "Proxies";
  base::mac::ScopedAEDesc<> openParams;
  OSStatus status = AECreateDesc('ptru',
                                 proxyPrefCommand,
                                 strlen(proxyPrefCommand),
                                 openParams.OutPointer());
  OSSTATUS_LOG_IF(ERROR, status != noErr, status)
      << "Failed to create open params";

  LSLaunchURLSpec launchSpec = { 0 };
  launchSpec.itemURLs = (CFArrayRef)itemsToOpen;
  launchSpec.passThruParams = openParams;
  launchSpec.launchFlags = kLSLaunchAsync | kLSLaunchDontAddToRecents;
  LSOpenFromURLSpec(&launchSpec, NULL);
}

void ShowManageSSLCertificates(content::WebContents* web_contents) {
  NSString* const kKeychainBundleId = @"com.apple.keychainaccess";
  [[NSWorkspace sharedWorkspace]
   launchAppWithBundleIdentifier:kKeychainBundleId
   options:0L
   additionalEventParamDescriptor:nil
   launchIdentifier:nil];
}

void ValidateSavedFonts(PrefService* prefs) {
  ValidateFontFamily(prefs, prefs::kWebKitSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitSansSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitFixedFontFamily);
}

}  // namespace settings_utils
