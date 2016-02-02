// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace options {

static void ValidateFontFamily(PrefService* prefs,
                               const char* family_pref_name) {
  // The native font settings dialog saved fonts by the font name, rather
  // than the family name.  This worked for the old dialog since
  // -[NSFont fontWithName:size] accepted a font or family name, but the
  // behavior was technically wrong.  Since we really need the family name for
  // the dom-ui options window, we will fix the saved preference if necessary.
  NSString *family_name =
      base::SysUTF8ToNSString(prefs->GetString(family_pref_name));
  NSFont *font = [NSFont fontWithName:family_name
                                 size:[NSFont systemFontSize]];
  if (font &&
      [[font familyName] caseInsensitiveCompare:family_name] != NSOrderedSame) {
    std::string new_family_name = base::SysNSStringToUTF8([font familyName]);
    prefs->SetString(family_pref_name, new_family_name);
  }
}

// static
void FontSettingsUtilities::ValidateSavedFonts(PrefService* prefs) {
  ValidateFontFamily(prefs, prefs::kWebKitSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitSansSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitFixedFontFamily);
}

}  // namespace options
