// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

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

ListValue* FontSettingsUtilities::GetFontsList() {
  ListValue* font_list = new ListValue;
  NSFontManager* fontManager = [NSFontManager sharedFontManager];
  NSArray* fonts = [fontManager availableFontFamilies];
  for (NSString* family_name in fonts) {
    NSString* localized_family_name =
        [fontManager localizedNameForFamily:family_name face:nil];
     ListValue* font_item = new ListValue();
    string16 family = base::SysNSStringToUTF16(family_name);
    font_item->Append(Value::CreateStringValue(family));
    string16 loc_family = base::SysNSStringToUTF16(localized_family_name);
    font_item->Append(Value::CreateStringValue(loc_family));
    font_list->Append(font_item);
  }
  return font_list;
}

void FontSettingsUtilities::ValidateSavedFonts(PrefService* prefs) {
  ValidateFontFamily(prefs, prefs::kWebKitSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitSansSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitFixedFontFamily);
}
