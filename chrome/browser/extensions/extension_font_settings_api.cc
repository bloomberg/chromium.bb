// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_font_settings_api.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_preference_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

#if defined(OS_WIN)
#include "ui/gfx/font.h"
#include "ui/gfx/platform_font_win.h"
#endif

namespace {

const char kCharsetKey[] = "charset";
const char kFontNameKey[] = "fontName";
const char kGenericFamilyKey[] = "genericFamily";
const char kLevelOfControlKey[] = "levelOfControl";
const char kLocalizedNameKey[] = "localizedName";
const char kPixelSizeKey[] = "pixelSize";
const char kScriptKey[] = "script";

const char kSetFromIncognitoError[] =
    "Can't modify regular settings from an incognito context.";

const char kOnDefaultCharacterSetChanged[] =
    "experimental.fontSettings.onDefaultCharacterSetChanged";
const char kOnDefaultFixedFontSizeChanged[] =
    "experimental.fontSettings.onDefaultFixedFontSizeChanged";
const char kOnDefaultFontSizeChanged[] =
    "experimental.fontSettings.onDefaultFontSizeChanged";
const char kOnFontChanged[] = "experimental.fontSettings.onFontChanged";
const char kOnMinimumFontSizeChanged[] =
    "experimental.fontSettings.onMinimumFontSizeChanged";

// Format for per-script font preference keys.
// E.g., "webkit.webprefs.fonts.standard.Hrkt"
const char kWebKitPerScriptFontPrefFormat[] = "webkit.webprefs.fonts.%s.%s";
const char kWebKitPerScriptFontPrefPrefix[] = "webkit.webprefs.fonts.";

// Format for global (non per-script) font preference keys.
// E.g., "webkit.webprefs.global.fixed_font_family"
// Note: there are two meanings of "global" here. The "Global" in the const name
// means "not per-script". The "global" in the key itself means "not per-tab"
// (per-profile).
const char kWebKitGlobalFontPrefFormat[] =
    "webkit.webprefs.global.%s_font_family";
const char kWebKitGlobalFontPrefPrefix[] = "webkit.webprefs.global.";
const char kWebKitGlobalFontPrefSuffix[] = "_font_family";

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

// Extracts the generic family and script from font name pref path |pref_path|.
bool ParseFontNamePrefPath(std::string pref_path,
                           std::string* generic_family,
                           std::string* script) {
  if (StartsWithASCII(pref_path, kWebKitPerScriptFontPrefPrefix, true)) {
    size_t start = strlen(kWebKitPerScriptFontPrefPrefix);
    size_t pos = pref_path.find('.', start);
    if (pos == std::string::npos || pos + 1 == pref_path.length())
      return false;
    *generic_family = pref_path.substr(start, pos - start);
    *script = pref_path.substr(pos + 1);
    return true;
  } else if (StartsWithASCII(pref_path, kWebKitGlobalFontPrefPrefix, true) &&
             EndsWith(pref_path, kWebKitGlobalFontPrefSuffix, true)) {
    size_t start = strlen(kWebKitGlobalFontPrefPrefix);
    size_t pos = pref_path.find('_', start);
    if (pos == std::string::npos || pos + 1 == pref_path.length())
      return false;
    *generic_family = pref_path.substr(start, pos - start);
    *script = "";
    return true;
  }
  return false;
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
void RegisterFontFamilyMapObserver(PrefChangeRegistrar* registrar,
                                   const char* map_name,
                                   content::NotificationObserver* obs) {
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::string pref_name = base::StringPrintf("%s.%s", map_name, script);
    registrar->Add(pref_name.c_str(), obs);
  }
}

}  // namespace

ExtensionFontSettingsEventRouter::ExtensionFontSettingsEventRouter(
    Profile* profile) : profile_(profile) {}

ExtensionFontSettingsEventRouter::~ExtensionFontSettingsEventRouter() {}

void ExtensionFontSettingsEventRouter::Init() {
  registrar_.Init(profile_->GetPrefs());

  AddPrefToObserve(prefs::kWebKitGlobalDefaultFixedFontSize,
                   kOnDefaultFixedFontSizeChanged,
                   kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitGlobalDefaultFontSize,
                   kOnDefaultFontSizeChanged,
                   kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitGlobalMinimumFontSize,
                   kOnMinimumFontSizeChanged,
                   kPixelSizeKey);
  AddPrefToObserve(prefs::kGlobalDefaultCharset,
                   kOnDefaultCharacterSetChanged,
                   kCharsetKey);

  registrar_.Add(prefs::kWebKitGlobalStandardFontFamily, this);
  registrar_.Add(prefs::kWebKitGlobalSerifFontFamily, this);
  registrar_.Add(prefs::kWebKitGlobalSansSerifFontFamily, this);
  registrar_.Add(prefs::kWebKitGlobalFixedFontFamily, this);
  registrar_.Add(prefs::kWebKitGlobalCursiveFontFamily, this);
  registrar_.Add(prefs::kWebKitGlobalFantasyFontFamily, this);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitStandardFontFamilyMap, this);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitSerifFontFamilyMap, this);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitSansSerifFontFamilyMap, this);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitFixedFontFamilyMap, this);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitCursiveFontFamilyMap, this);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitFantasyFontFamilyMap, this);
}

void ExtensionFontSettingsEventRouter::AddPrefToObserve(const char* pref_name,
                                                        const char* event_name,
                                                        const char* key) {
  registrar_.Add(pref_name, this);
  pref_event_map_[pref_name] = std::make_pair(event_name, key);
}

void ExtensionFontSettingsEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_PREF_CHANGED) {
    NOTREACHED();
    return;
  }

  PrefService* pref_service = content::Source<PrefService>(source).ptr();
  bool incognito = (pref_service != profile_->GetPrefs());
  // We're only observing pref changes on the regular profile.
  DCHECK(!incognito);
  const std::string* pref_name =
      content::Details<const std::string>(details).ptr();

  PrefEventMap::iterator iter = pref_event_map_.find(*pref_name);
  if (iter != pref_event_map_.end()) {
    const std::string& event_name = iter->second.first;
    const std::string& key = iter->second.second;
    OnFontPrefChanged(pref_service, *pref_name, event_name, key, incognito);
    return;
  }

  std::string generic_family;
  std::string script;
  if (ParseFontNamePrefPath(*pref_name, &generic_family, &script)) {
    OnFontNamePrefChanged(pref_service, *pref_name, generic_family, script,
                          incognito);
    return;
  }

  NOTREACHED();
}

void ExtensionFontSettingsEventRouter::OnFontNamePrefChanged(
    PrefService* pref_service,
    const std::string& pref_name,
    const std::string& generic_family,
    const std::string& script,
    bool incognito) {
  const PrefService::Preference* pref = pref_service->FindPreference(
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
  dict->SetString(kFontNameKey, font_name);
  dict->SetString(kGenericFamilyKey, generic_family);
  if (!script.empty())
    dict->SetString(kScriptKey, script);

  extension_preference_helpers::DispatchEventToExtensions(
      profile_,
      kOnFontChanged,
      &args,
      ExtensionAPIPermission::kExperimental,
      incognito,
      pref_name);
}

void ExtensionFontSettingsEventRouter::OnFontPrefChanged(
    PrefService* pref_service,
    const std::string& pref_name,
    const std::string& event_name,
    const std::string& key,
    bool incognito) {
  const PrefService::Preference* pref = pref_service->FindPreference(
      pref_name.c_str());
  CHECK(pref);

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  args.Append(dict);
  dict->Set(key, pref->GetValue()->DeepCopy());

  extension_preference_helpers::DispatchEventToExtensions(
      profile_,
      event_name,
      &args,
      ExtensionAPIPermission::kExperimental,
      incognito,
      pref_name);
}

bool ClearFontFunction::RunImpl() {
  if (profile_->IsOffTheRecord()) {
    error_ = kSetFromIncognitoError;
    return false;
  }

  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  std::string pref_path;
  EXTENSION_FUNCTION_VALIDATE(GetFontNamePrefPath(details, &pref_path));

  // Ensure |pref_path| really is for a registered per-script font pref.
  EXTENSION_FUNCTION_VALIDATE(
      profile_->GetPrefs()->FindPreference(pref_path.c_str()));

  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
  prefs->RemoveExtensionControlledPref(extension_id(),
                                       pref_path.c_str(),
                                       kExtensionPrefsScopeRegular);
  return true;
}

bool GetFontFunction::RunImpl() {
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
  font_name = MaybeGetLocalizedFontName(font_name);

  // We don't support incognito-specific font prefs, so don't consider them when
  // getting level of control.
  const bool kIncognito = false;
  std::string level_of_control =
      extension_preference_helpers::GetLevelOfControl(profile_,
                                                      extension_id(),
                                                      pref_path,
                                                      kIncognito);

  DictionaryValue* result = new DictionaryValue();
  result->SetString(kFontNameKey, font_name);
  result->SetString(kLevelOfControlKey, level_of_control);
  result_.reset(result);
  return true;
}

bool SetFontFunction::RunImpl() {
  if (profile_->IsOffTheRecord()) {
    error_ = kSetFromIncognitoError;
    return false;
  }

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

bool ClearFontPrefExtensionFunction::RunImpl() {
  if (profile_->IsOffTheRecord()) {
    error_ = kSetFromIncognitoError;
    return false;
  }

  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
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
      extension_preference_helpers::GetLevelOfControl(profile_,
                                                      extension_id(),
                                                      GetPrefName(),
                                                      kIncognito);

  DictionaryValue* result = new DictionaryValue();
  result->Set(GetKey(), pref->GetValue()->DeepCopy());
  result->SetString(kLevelOfControlKey, level_of_control);
  result_.reset(result);
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

  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
  prefs->SetExtensionControlledPref(extension_id(),
                                    GetPrefName(),
                                    kExtensionPrefsScopeRegular,
                                    value->DeepCopy());
  return true;
}

const char* ClearDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFontSize;
}

const char* GetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFontSize;
}

const char* GetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* SetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFontSize;
}

const char* SetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* ClearDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFixedFontSize;
}

const char* GetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFixedFontSize;
}

const char* GetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* SetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalDefaultFixedFontSize;
}

const char* SetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* ClearMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalMinimumFontSize;
}

const char* GetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalMinimumFontSize;
}

const char* GetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* SetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitGlobalMinimumFontSize;
}

const char* SetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* ClearDefaultCharacterSetFunction::GetPrefName() {
  return prefs::kGlobalDefaultCharset;
}

const char* GetDefaultCharacterSetFunction::GetPrefName() {
  return prefs::kGlobalDefaultCharset;
}

const char* GetDefaultCharacterSetFunction::GetKey() {
  return kCharsetKey;
}

const char* SetDefaultCharacterSetFunction::GetPrefName() {
  return prefs::kGlobalDefaultCharset;
}

const char* SetDefaultCharacterSetFunction::GetKey() {
  return kCharsetKey;
}
