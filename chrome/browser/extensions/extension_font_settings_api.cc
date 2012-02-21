// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_font_settings_api.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_preference_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace {

const char kGenericFamilyKey[] = "genericFamily";
const char kFontNameKey[] = "fontName";
const char kScriptKey[] = "script";

// Format for per-script font preference keys.
// E.g., "webkit.webprefs.fonts.standard.Hrkt"
const char kWebKitPerScriptFontPrefFormat[] = "webkit.webprefs.fonts.%s.%s";

// Format for global (non per-script) font preference keys.
// E.g., "webkit.webprefs.global.fixed_font_family"
// Note: there are two meanings of "global" here. The "Global" in the const name
// means "not per-script". The "global" in the key itself means "not per-tab"
// (per-profile).
const char kWebKitGlobalFontPrefFormat[] =
    "webkit.webprefs.global.%s_font_family";

}  // namespace

bool GetFontNameFunction::RunImpl() {
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  std::string generic_family;
  EXTENSION_FUNCTION_VALIDATE(details->GetString(kGenericFamilyKey,
                                                 &generic_family));

  std::string pref_path;
  if (details->HasKey(kScriptKey)) {
    std::string script;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kScriptKey, &script));
    pref_path = base::StringPrintf(kWebKitPerScriptFontPrefFormat,
                                   generic_family.c_str(),
                                   script.c_str());
  } else {
    pref_path = base::StringPrintf(kWebKitGlobalFontPrefFormat,
                                   generic_family.c_str());
  }

  PrefService* prefs = profile_->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(pref_path.c_str());
  std::string font_name;
  EXTENSION_FUNCTION_VALIDATE(
      pref && pref->GetValue()->GetAsString(&font_name));

  DictionaryValue* result = new DictionaryValue();
  result->SetString(kFontNameKey, font_name);
  result_.reset(result);
  return true;
}
