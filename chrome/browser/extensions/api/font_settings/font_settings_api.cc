// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font Settings Extension API implementation.

#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/font_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names_util.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/error_utils.h"

#if defined(OS_WIN)
#include "ui/gfx/font.h"
#include "ui/gfx/platform_font_win.h"
#endif

namespace extensions {

namespace fonts = api::font_settings;

namespace {

const char kFontIdKey[] = "fontId";
const char kGenericFamilyKey[] = "genericFamily";
const char kLevelOfControlKey[] = "levelOfControl";
const char kDisplayNameKey[] = "displayName";
const char kPixelSizeKey[] = "pixelSize";
const char kScriptKey[] = "script";

const char kSetFromIncognitoError[] =
    "Can't modify regular settings from an incognito context.";

const char kOnDefaultFixedFontSizeChanged[] =
    "fontSettings.onDefaultFixedFontSizeChanged";
const char kOnDefaultFontSizeChanged[] =
    "fontSettings.onDefaultFontSizeChanged";
const char kOnFontChanged[] = "fontSettings.onFontChanged";
const char kOnMinimumFontSizeChanged[] =
    "fontSettings.onMinimumFontSizeChanged";

// Format for font name preference paths.
const char kWebKitFontPrefFormat[] = "webkit.webprefs.fonts.%s.%s";

// Gets the font name preference path for |generic_family| and |script|. If
// |script| is NULL, uses prefs::kWebKitCommonScript.
std::string GetFontNamePrefPath(fonts::GenericFamily generic_family_enum,
                                fonts::ScriptCode script_enum) {
  std::string script = fonts::ToString(script_enum);
  if (script.empty())
    script = prefs::kWebKitCommonScript;
  std::string generic_family = fonts::ToString(generic_family_enum);
  return StringPrintf(kWebKitFontPrefFormat,
                      generic_family.c_str(),
                      script.c_str());
}

// Returns the localized name of a font so that it can be matched within the
// list of system fonts. On Windows, the list of system fonts has names only
// for the system locale, but the pref value may be in the English name.
std::string MaybeGetLocalizedFontName(const std::string& font_name) {
#if defined(OS_WIN)
  if (!font_name.empty()) {
    gfx::Font font(font_name, 12);  // dummy font size
    return static_cast<gfx::PlatformFontWin*>(font.platform_font())->
        GetLocalizedFontName();
  }
#endif
  return font_name;
}

// Registers |obs| to observe per-script font prefs under the path |map_name|.
void RegisterFontFamilyMapObserver(
    PrefChangeRegistrar* registrar,
    const char* map_name,
    const PrefChangeRegistrar::NamedChangeCallback& callback) {
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::string pref_name = base::StringPrintf("%s.%s", map_name, script);
    registrar->Add(pref_name.c_str(), callback);
  }
}

}  // namespace

FontSettingsEventRouter::FontSettingsEventRouter(
    Profile* profile) : profile_(profile) {
  registrar_.Init(profile_->GetPrefs());

  AddPrefToObserve(prefs::kWebKitDefaultFixedFontSize,
                   kOnDefaultFixedFontSizeChanged,
                   kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitDefaultFontSize,
                   kOnDefaultFontSizeChanged,
                   kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitMinimumFontSize,
                   kOnMinimumFontSizeChanged,
                   kPixelSizeKey);

  PrefChangeRegistrar::NamedChangeCallback callback =
      base::Bind(&FontSettingsEventRouter::OnFontFamilyMapPrefChanged,
                 base::Unretained(this));
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitStandardFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitSerifFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitSansSerifFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitFixedFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitCursiveFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitFantasyFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitPictographFontFamilyMap,
                                callback);
}

FontSettingsEventRouter::~FontSettingsEventRouter() {}

void FontSettingsEventRouter::AddPrefToObserve(const char* pref_name,
                                               const char* event_name,
                                               const char* key) {
  registrar_.Add(pref_name,
                 base::Bind(&FontSettingsEventRouter::OnFontPrefChanged,
                            base::Unretained(this),
                            event_name, key));
}

void FontSettingsEventRouter::OnFontFamilyMapPrefChanged(
    const std::string& pref_name) {
  std::string generic_family;
  std::string script;
  if (pref_names_util::ParseFontNamePrefPath(pref_name, &generic_family,
                                             &script)) {
    OnFontNamePrefChanged(pref_name, generic_family, script);
    return;
  }

  NOTREACHED();
}

void FontSettingsEventRouter::OnFontNamePrefChanged(
    const std::string& pref_name,
    const std::string& generic_family,
    const std::string& script) {
  const PrefServiceBase::Preference* pref = registrar_.prefs()->FindPreference(
      pref_name.c_str());
  CHECK(pref);

  std::string font_name;
  if (!pref->GetValue()->GetAsString(&font_name)) {
    NOTREACHED();
    return;
  }
  font_name = MaybeGetLocalizedFontName(font_name);

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  args.Append(dict);
  dict->SetString(kFontIdKey, font_name);
  dict->SetString(kGenericFamilyKey, generic_family);
  dict->SetString(kScriptKey, script);

  extensions::preference_helpers::DispatchEventToExtensions(
      profile_,
      kOnFontChanged,
      &args,
      APIPermission::kFontSettings,
      false,
      pref_name);
}

void FontSettingsEventRouter::OnFontPrefChanged(
    const std::string& event_name,
    const std::string& key,
    const std::string& pref_name) {
  const PrefServiceBase::Preference* pref = registrar_.prefs()->FindPreference(
      pref_name.c_str());
  CHECK(pref);

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  args.Append(dict);
  dict->Set(key, pref->GetValue()->DeepCopy());

  extensions::preference_helpers::DispatchEventToExtensions(
      profile_,
      event_name,
      &args,
      APIPermission::kFontSettings,
      false,
      pref_name);
}

FontSettingsAPI::FontSettingsAPI(Profile* profile)
    : font_settings_event_router_(new FontSettingsEventRouter(profile)) {
}

FontSettingsAPI::~FontSettingsAPI() {
}

bool FontSettingsClearFontFunction::RunImpl() {
  if (profile_->IsOffTheRecord()) {
    error_ = kSetFromIncognitoError;
    return false;
  }

  scoped_ptr<fonts::ClearFont::Params> params(
      fonts::ClearFont::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  // Ensure |pref_path| really is for a registered per-script font pref.
  EXTENSION_FUNCTION_VALIDATE(
      profile_->GetPrefs()->FindPreference(pref_path.c_str()));

  ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->extension_prefs();
  prefs->RemoveExtensionControlledPref(extension_id(),
                                       pref_path.c_str(),
                                       kExtensionPrefsScopeRegular);
  return true;
}

bool FontSettingsGetFontFunction::RunImpl() {
  scoped_ptr<fonts::GetFont::Params> params(
      fonts::GetFont::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  PrefService* prefs = profile_->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(pref_path.c_str());

  std::string font_name;
  EXTENSION_FUNCTION_VALIDATE(
      pref && pref->GetValue()->GetAsString(&font_name));
  font_name = MaybeGetLocalizedFontName(font_name);

  // We don't support incognito-specific font prefs, so don't consider them when
  // getting level of control.
  const bool kIncognito = false;
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(profile_,
                                                        extension_id(),
                                                        pref_path,
                                                        kIncognito);

  DictionaryValue* result = new DictionaryValue();
  result->SetString(kFontIdKey, font_name);
  result->SetString(kLevelOfControlKey, level_of_control);
  SetResult(result);
  return true;
}

bool FontSettingsSetFontFunction::RunImpl() {
  if (profile_->IsOffTheRecord()) {
    error_ = kSetFromIncognitoError;
    return false;
  }

  scoped_ptr<fonts::SetFont::Params> params(
      fonts::SetFont::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  // Ensure |pref_path| really is for a registered font pref.
  EXTENSION_FUNCTION_VALIDATE(
      profile_->GetPrefs()->FindPreference(pref_path.c_str()));

  ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->extension_prefs();
  prefs->SetExtensionControlledPref(
      extension_id(),
      pref_path.c_str(),
      kExtensionPrefsScopeRegular,
      Value::CreateStringValue(params->details.font_id));
  return true;
}

bool FontSettingsGetFontListFunction::RunImpl() {
  content::GetFontListAsync(
      Bind(&FontSettingsGetFontListFunction::FontListHasLoaded, this));
  return true;
}

void FontSettingsGetFontListFunction::FontListHasLoaded(
    scoped_ptr<ListValue> list) {
  bool success = CopyFontsToResult(list.get());
  SendResponse(success);
}

bool FontSettingsGetFontListFunction::CopyFontsToResult(ListValue* fonts) {
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
    font_name->Set(kFontIdKey, Value::CreateStringValue(name));
    font_name->Set(kDisplayNameKey, Value::CreateStringValue(localized_name));
    result->Append(font_name);
  }

  SetResult(result.release());
  return true;
}

bool ClearFontPrefExtensionFunction::RunImpl() {
  if (profile_->IsOffTheRecord()) {
    error_ = kSetFromIncognitoError;
    return false;
  }

  ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->extension_prefs();
  prefs->RemoveExtensionControlledPref(extension_id(),
                                       GetPrefName(),
                                       kExtensionPrefsScopeRegular);
  return true;
}

bool GetFontPrefExtensionFunction::RunImpl() {
  PrefService* prefs = profile_->GetPrefs();
  const PrefService::Preference* pref = prefs->FindPreference(GetPrefName());
  EXTENSION_FUNCTION_VALIDATE(pref);

  // We don't support incognito-specific font prefs, so don't consider them when
  // getting level of control.
  const bool kIncognito = false;

  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(profile_,
                                                        extension_id(),
                                                        GetPrefName(),
                                                        kIncognito);

  DictionaryValue* result = new DictionaryValue();
  result->Set(GetKey(), pref->GetValue()->DeepCopy());
  result->SetString(kLevelOfControlKey, level_of_control);
  SetResult(result);
  return true;
}

bool SetFontPrefExtensionFunction::RunImpl() {
  if (profile_->IsOffTheRecord()) {
    error_ = kSetFromIncognitoError;
    return false;
  }

  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  Value* value;
  EXTENSION_FUNCTION_VALIDATE(details->Get(GetKey(), &value));

  ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->extension_prefs();
  prefs->SetExtensionControlledPref(extension_id(),
                                    GetPrefName(),
                                    kExtensionPrefsScopeRegular,
                                    value->DeepCopy());
  return true;
}

const char* FontSettingsClearDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsGetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsGetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsSetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsClearDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsGetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsGetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsSetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsClearMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsGetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsGetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsSetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

}  // namespace extensions
