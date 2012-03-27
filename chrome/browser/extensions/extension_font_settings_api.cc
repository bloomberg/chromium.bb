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
#include "chrome/common/pref_names.h"
#include "content/public/browser/font_list_async.h"

namespace {

const char kGenericFamilyKey[] = "genericFamily";
const char kFontNameKey[] = "fontName";
const char kLocalizedNameKey[] = "localizedName";
const char kPixelSizeKey[] = "pixelSize";
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

// Gets the font name preference path from |details| which contains key
// |kGenericFamilyKey| and optionally |kScriptKey|.
bool GetFontNamePrefPath(DictionaryValue* details, std::string* pref_path) {
  std::string generic_family;
  if (!details->GetString(kGenericFamilyKey, &generic_family))
    return false;

  if (details->HasKey(kScriptKey)) {
    std::string script;
    if (!details->GetString(kScriptKey, &script))
      return false;
    *pref_path = StringPrintf(kWebKitPerScriptFontPrefFormat,
                                   generic_family.c_str(),
                                   script.c_str());
  } else {
    *pref_path = StringPrintf(kWebKitGlobalFontPrefFormat,
                                   generic_family.c_str());
  }

  return true;
}

}  // namespace

bool GetFontNameFunction::RunImpl() {
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  std::string pref_path;
  EXTENSION_FUNCTION_VALIDATE(GetFontNamePrefPath(details, &pref_path));

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

bool SetFontNameFunction::RunImpl() {
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  std::string pref_path;
  EXTENSION_FUNCTION_VALIDATE(GetFontNamePrefPath(details, &pref_path));

  std::string font_name;
  EXTENSION_FUNCTION_VALIDATE(details->GetString(kFontNameKey, &font_name));

  // Ensure |pref_path| really is for a registered per-script font pref.
  EXTENSION_FUNCTION_VALIDATE(
      profile_->GetPrefs()->FindPreference(pref_path.c_str()));

  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
  prefs->SetExtensionControlledPref(extension_id(), pref_path.c_str(),
                                    kExtensionPrefsScopeRegular,
                                    Value::CreateStringValue(font_name));
  return true;
}

bool GetFontListFunction::RunImpl() {
  content::GetFontListAsync(
      Bind(&GetFontListFunction::FontListHasLoaded, this));
  return true;
}

void GetFontListFunction::FontListHasLoaded(scoped_ptr<ListValue> list) {
  bool success = CopyFontsToResult(list.get());
  SendResponse(success);
}

bool GetFontListFunction::CopyFontsToResult(ListValue* fonts) {
  scoped_ptr<ListValue> result(new ListValue());
  for (ListValue::iterator it = fonts->begin(); it != fonts->end(); ++it) {
    ListValue* font_list_value;
    if (!(*it)->GetAsList(&font_list_value)) {
      NOTREACHED();
      return false;
    }

    std::string name;
    if (!font_list_value->GetString(0, &name)) {
      NOTREACHED();
      return false;
    }

    std::string localized_name;
    if (!font_list_value->GetString(1, &localized_name)) {
      NOTREACHED();
      return false;
    }

    DictionaryValue* font_name = new DictionaryValue();
    font_name->Set(kFontNameKey, Value::CreateStringValue(name));
    font_name->Set(kLocalizedNameKey, Value::CreateStringValue(localized_name));
    result->Append(font_name);
  }

  result_.reset(result.release());
  return true;
}

bool GetFontSizeExtensionFunction::RunImpl() {
  PrefService* prefs = profile_->GetPrefs();
  int size = prefs->GetInteger(GetPrefName());

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kPixelSizeKey, size);
  result_.reset(result);
  return true;
}

bool SetFontSizeExtensionFunction::RunImpl() {
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  int size;
  EXTENSION_FUNCTION_VALIDATE(details->GetInteger(kPixelSizeKey, &size));

  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
  prefs->SetExtensionControlledPref(extension_id(),
                                    GetPrefName(),
                                    kExtensionPrefsScopeRegular,
                                    Value::CreateIntegerValue(size));
  return true;
}

const char* GetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFontSize;
}

const char* SetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFontSize;
}

const char* GetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFixedFontSize;
}

const char* SetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFixedFontSize;
}

const char* GetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalMinimumFontSize;
}

const char* SetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalMinimumFontSize;
}
