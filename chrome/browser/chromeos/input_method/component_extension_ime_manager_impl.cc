// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ime/extension_ime_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

struct WhitelistedComponentExtensionIME {
  const char* id;
  int manifest_resource_id;
} whitelisted_component_extension[] = {
      {// ChromeOS Hangul Input.
       extension_ime_util::kHangulExtensionId, IDR_HANGUL_MANIFEST,
      },
#if defined(GOOGLE_CHROME_BUILD)
      {// Official Google XKB Input.
       extension_ime_util::kXkbExtensionId, IDR_GOOGLE_XKB_MANIFEST,
      },
      {// Google input tools.
       extension_ime_util::kT13nExtensionId, IDR_GOOGLE_INPUT_TOOLS_MANIFEST,
      },
#else
      {// Open-sourced ChromeOS xkb extension.
       extension_ime_util::kXkbExtensionId, IDR_XKB_MANIFEST,
      },
      {// Open-sourced ChromeOS Keyboards extension.
       extension_ime_util::kM17nExtensionId, IDR_M17N_MANIFEST,
      },
      {// Open-sourced Pinyin Chinese Input Method.
       extension_ime_util::kChinesePinyinExtensionId, IDR_PINYIN_MANIFEST,
      },
      {// Open-sourced Zhuyin Chinese Input Method.
       extension_ime_util::kChineseZhuyinExtensionId, IDR_ZHUYIN_MANIFEST,
      },
      {// Open-sourced Cangjie Chinese Input Method.
       extension_ime_util::kChineseCangjieExtensionId, IDR_CANGJIE_MANIFEST,
      },
      {// Japanese Mozc Input.
       extension_ime_util::kMozcExtensionId, IDR_MOZC_MANIFEST,
      },
#endif
      {// Braille hardware keyboard IME that works together with ChromeVox.
       extension_misc::kBrailleImeExtensionId, IDR_BRAILLE_MANIFEST,
      },
};

const struct InputMethodNameMap {
  const char* message_name;
  int resource_id;
  bool operator<(const InputMethodNameMap& other) const {
    return strcmp(message_name, other.message_name) < 0;
  }
} kInputMethodNameMap[] = {
      {"__MSG_INPUTMETHOD_ARRAY__", IDS_IME_NAME_INPUTMETHOD_ARRAY},
      {"__MSG_INPUTMETHOD_CANGJIE__", IDS_IME_NAME_INPUTMETHOD_CANGJIE},
      {"__MSG_INPUTMETHOD_DAYI__", IDS_IME_NAME_INPUTMETHOD_DAYI},
      {"__MSG_INPUTMETHOD_HANGUL_2_SET__",
       IDS_IME_NAME_INPUTMETHOD_HANGUL_2_SET},
      {"__MSG_INPUTMETHOD_HANGUL_3_SET_390__",
       IDS_IME_NAME_INPUTMETHOD_HANGUL_3_SET_390},
      {"__MSG_INPUTMETHOD_HANGUL_3_SET_FINAL__",
       IDS_IME_NAME_INPUTMETHOD_HANGUL_3_SET_FINAL},
      {"__MSG_INPUTMETHOD_HANGUL_3_SET_NO_SHIFT__",
       IDS_IME_NAME_INPUTMETHOD_HANGUL_3_SET_NO_SHIFT},
      {"__MSG_INPUTMETHOD_HANGUL_AHNMATAE__",
       IDS_IME_NAME_INPUTMETHOD_HANGUL_AHNMATAE},
      {"__MSG_INPUTMETHOD_HANGUL_ROMAJA__",
       IDS_IME_NAME_INPUTMETHOD_HANGUL_ROMAJA},
      {"__MSG_INPUTMETHOD_MOZC_JP__", IDS_IME_NAME_INPUTMETHOD_MOZC_JP},
      {"__MSG_INPUTMETHOD_MOZC_US__", IDS_IME_NAME_INPUTMETHOD_MOZC_US},
      {"__MSG_INPUTMETHOD_PINYIN__", IDS_IME_NAME_INPUTMETHOD_PINYIN},
      {"__MSG_INPUTMETHOD_QUICK__", IDS_IME_NAME_INPUTMETHOD_QUICK},
      {"__MSG_INPUTMETHOD_TRADITIONAL_PINYIN__",
       IDS_IME_NAME_INPUTMETHOD_TRADITIONAL_PINYIN},
      {"__MSG_INPUTMETHOD_WUBI__", IDS_IME_NAME_INPUTMETHOD_WUBI},
      {"__MSG_INPUTMETHOD_ZHUYIN__", IDS_IME_NAME_INPUTMETHOD_ZHUYIN},
      {"__MSG_KEYBOARD_ARABIC__", IDS_IME_NAME_KEYBOARD_ARABIC},
      {"__MSG_KEYBOARD_ARMENIAN_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_ARMENIAN_PHONETIC},
      {"__MSG_KEYBOARD_BELARUSIAN__", IDS_IME_NAME_KEYBOARD_BELARUSIAN},
      {"__MSG_KEYBOARD_BELGIAN__", IDS_IME_NAME_KEYBOARD_BELGIAN},
      {"__MSG_KEYBOARD_BENGALI_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_BENGALI_PHONETIC},
      {"__MSG_KEYBOARD_BRAZILIAN__", IDS_IME_NAME_KEYBOARD_BRAZILIAN},
      {"__MSG_KEYBOARD_BULGARIAN_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_BULGARIAN_PHONETIC},
      {"__MSG_KEYBOARD_BULGARIAN__", IDS_IME_NAME_KEYBOARD_BULGARIAN},
      {"__MSG_KEYBOARD_CANADIAN_ENGLISH__",
       IDS_IME_NAME_KEYBOARD_CANADIAN_ENGLISH},
      {"__MSG_KEYBOARD_CANADIAN_FRENCH__",
       IDS_IME_NAME_KEYBOARD_CANADIAN_FRENCH},
      {"__MSG_KEYBOARD_CANADIAN_MULTILINGUAL__",
       IDS_IME_NAME_KEYBOARD_CANADIAN_MULTILINGUAL},
      {"__MSG_KEYBOARD_CATALAN__", IDS_IME_NAME_KEYBOARD_CATALAN},
      {"__MSG_KEYBOARD_CROATIAN__", IDS_IME_NAME_KEYBOARD_CROATIAN},
      {"__MSG_KEYBOARD_CZECH_QWERTY__", IDS_IME_NAME_KEYBOARD_CZECH_QWERTY},
      {"__MSG_KEYBOARD_CZECH__", IDS_IME_NAME_KEYBOARD_CZECH},
      {"__MSG_KEYBOARD_DANISH__", IDS_IME_NAME_KEYBOARD_DANISH},
      {"__MSG_KEYBOARD_DEVANAGARI_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_DEVANAGARI_PHONETIC},
      {"__MSG_KEYBOARD_ESTONIAN__", IDS_IME_NAME_KEYBOARD_ESTONIAN},
      {"__MSG_KEYBOARD_ETHIOPIC__", IDS_IME_NAME_KEYBOARD_ETHIOPIC},
      {"__MSG_KEYBOARD_FINNISH__", IDS_IME_NAME_KEYBOARD_FINNISH},
      {"__MSG_KEYBOARD_FRENCH__", IDS_IME_NAME_KEYBOARD_FRENCH},
      {"__MSG_KEYBOARD_GEORGIAN__", IDS_IME_NAME_KEYBOARD_GEORGIAN},
      {"__MSG_KEYBOARD_GERMAN_NEO_2__", IDS_IME_NAME_KEYBOARD_GERMAN_NEO_2},
      {"__MSG_KEYBOARD_GERMAN__", IDS_IME_NAME_KEYBOARD_GERMAN},
      {"__MSG_KEYBOARD_GREEK__", IDS_IME_NAME_KEYBOARD_GREEK},
      {"__MSG_KEYBOARD_GUJARATI_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_GUJARATI_PHONETIC},
      {"__MSG_KEYBOARD_HEBREW__", IDS_IME_NAME_KEYBOARD_HEBREW},
      {"__MSG_KEYBOARD_HUNGARIAN__", IDS_IME_NAME_KEYBOARD_HUNGARIAN},
      {"__MSG_KEYBOARD_ICELANDIC__", IDS_IME_NAME_KEYBOARD_ICELANDIC},
      {"__MSG_KEYBOARD_IRISH__", IDS_IME_NAME_KEYBOARD_IRISH},
      {"__MSG_KEYBOARD_ITALIAN__", IDS_IME_NAME_KEYBOARD_ITALIAN},
      {"__MSG_KEYBOARD_JAPANESE__", IDS_IME_NAME_KEYBOARD_JAPANESE},
      {"__MSG_KEYBOARD_KANNADA_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_KANNADA_PHONETIC},
      {"__MSG_KEYBOARD_KHMER__", IDS_IME_NAME_KEYBOARD_KHMER},
      {"__MSG_KEYBOARD_LAO__", IDS_IME_NAME_KEYBOARD_LAO},
      {"__MSG_KEYBOARD_LATIN_AMERICAN__", IDS_IME_NAME_KEYBOARD_LATIN_AMERICAN},
      {"__MSG_KEYBOARD_LATVIAN__", IDS_IME_NAME_KEYBOARD_LATVIAN},
      {"__MSG_KEYBOARD_LITHUANIAN__", IDS_IME_NAME_KEYBOARD_LITHUANIAN},
      {"__MSG_KEYBOARD_MALAYALAM_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_MALAYALAM_PHONETIC},
      {"__MSG_KEYBOARD_MONGOLIAN__", IDS_IME_NAME_KEYBOARD_MONGOLIAN},
      {"__MSG_KEYBOARD_MYANMAR_MYANSAN__",
       IDS_IME_NAME_KEYBOARD_MYANMAR_MYANSAN},
      {"__MSG_KEYBOARD_MYANMAR__", IDS_IME_NAME_KEYBOARD_MYANMAR},
      {"__MSG_KEYBOARD_NEPALI_INSCRIPT__",
       IDS_IME_NAME_KEYBOARD_NEPALI_INSCRIPT},
      {"__MSG_KEYBOARD_NEPALI_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_NEPALI_PHONETIC},
      {"__MSG_KEYBOARD_NORWEGIAN__", IDS_IME_NAME_KEYBOARD_NORWEGIAN},
      {"__MSG_KEYBOARD_PERSIAN__", IDS_IME_NAME_KEYBOARD_PERSIAN},
      {"__MSG_KEYBOARD_POLISH__", IDS_IME_NAME_KEYBOARD_POLISH},
      {"__MSG_KEYBOARD_PORTUGUESE__", IDS_IME_NAME_KEYBOARD_PORTUGUESE},
      {"__MSG_KEYBOARD_ROMANIAN__", IDS_IME_NAME_KEYBOARD_ROMANIAN},
      {"__MSG_KEYBOARD_RUSSIAN_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_RUSSIAN_PHONETIC},
      {"__MSG_KEYBOARD_RUSSIAN__", IDS_IME_NAME_KEYBOARD_RUSSIAN},
      {"__MSG_KEYBOARD_SERBIAN__", IDS_IME_NAME_KEYBOARD_SERBIAN},
      {"__MSG_KEYBOARD_SINHALA__", IDS_IME_NAME_KEYBOARD_SINHALA},
      {"__MSG_KEYBOARD_SLOVAKIAN__", IDS_IME_NAME_KEYBOARD_SLOVAKIAN},
      {"__MSG_KEYBOARD_SLOVENIAN__", IDS_IME_NAME_KEYBOARD_SLOVENIAN},
      {"__MSG_KEYBOARD_SORANIKURDISH_AR__",
       IDS_IME_NAME_KEYBOARD_SORANIKURDISH_AR},
      {"__MSG_KEYBOARD_SORANIKURDISH_EN__",
       IDS_IME_NAME_KEYBOARD_SORANIKURDISH_EN},
      {"__MSG_KEYBOARD_SPANISH__", IDS_IME_NAME_KEYBOARD_SPANISH},
      {"__MSG_KEYBOARD_SWEDISH__", IDS_IME_NAME_KEYBOARD_SWEDISH},
      {"__MSG_KEYBOARD_SWISS_FRENCH__", IDS_IME_NAME_KEYBOARD_SWISS_FRENCH},
      {"__MSG_KEYBOARD_SWISS__", IDS_IME_NAME_KEYBOARD_SWISS},
      {"__MSG_KEYBOARD_TAMIL_INSCRIPT__", IDS_IME_NAME_KEYBOARD_TAMIL_INSCRIPT},
      {"__MSG_KEYBOARD_TAMIL_ITRANS__", IDS_IME_NAME_KEYBOARD_TAMIL_ITRANS},
      {"__MSG_KEYBOARD_TAMIL_PHONETIC__", IDS_IME_NAME_KEYBOARD_TAMIL_PHONETIC},
      {"__MSG_KEYBOARD_TAMIL_TAMIL99__", IDS_IME_NAME_KEYBOARD_TAMIL_TAMIL99},
      {"__MSG_KEYBOARD_TAMIL_TYPEWRITER__",
       IDS_IME_NAME_KEYBOARD_TAMIL_TYPEWRITER},
      {"__MSG_KEYBOARD_TELUGU_PHONETIC__",
       IDS_IME_NAME_KEYBOARD_TELUGU_PHONETIC},
      {"__MSG_KEYBOARD_THAI_KEDMANEE__", IDS_IME_NAME_KEYBOARD_THAI_KEDMANEE},
      {"__MSG_KEYBOARD_THAI_PATTACHOTE__",
       IDS_IME_NAME_KEYBOARD_THAI_PATTACHOTE},
      {"__MSG_KEYBOARD_THAI_TIS__", IDS_IME_NAME_KEYBOARD_THAI_TIS},
      {"__MSG_KEYBOARD_TURKISH__", IDS_IME_NAME_KEYBOARD_TURKISH},
      {"__MSG_KEYBOARD_UKRAINIAN__", IDS_IME_NAME_KEYBOARD_UKRAINIAN},
      {"__MSG_KEYBOARD_UK_DVORAK__", IDS_IME_NAME_KEYBOARD_UK_DVORAK},
      {"__MSG_KEYBOARD_UK__", IDS_IME_NAME_KEYBOARD_UK},
      {"__MSG_KEYBOARD_US_COLEMAK__", IDS_IME_NAME_KEYBOARD_US_COLEMAK},
      {"__MSG_KEYBOARD_US_DVORAK__", IDS_IME_NAME_KEYBOARD_US_DVORAK},
      {"__MSG_KEYBOARD_US_EXTENDED__", IDS_IME_NAME_KEYBOARD_US_EXTENDED},
      {"__MSG_KEYBOARD_US_INTERNATIONAL__",
       IDS_IME_NAME_KEYBOARD_US_INTERNATIONAL},
      {"__MSG_KEYBOARD_US__", IDS_IME_NAME_KEYBOARD_US},
      {"__MSG_KEYBOARD_VIETNAMESE_TCVN__",
       IDS_IME_NAME_KEYBOARD_VIETNAMESE_TCVN},
      {"__MSG_KEYBOARD_VIETNAMESE_TELEX__",
       IDS_IME_NAME_KEYBOARD_VIETNAMESE_TELEX},
      {"__MSG_KEYBOARD_VIETNAMESE_VIQR__",
       IDS_IME_NAME_KEYBOARD_VIETNAMESE_VIQR},
      {"__MSG_KEYBOARD_VIETNAMESE_VNI__", IDS_IME_NAME_KEYBOARD_VIETNAMESE_VNI},
      {"__MSG_TRANSLITERATION_AM__", IDS_IME_NAME_TRANSLITERATION_AM},
      {"__MSG_TRANSLITERATION_AR__", IDS_IME_NAME_TRANSLITERATION_AR},
      {"__MSG_TRANSLITERATION_BN__", IDS_IME_NAME_TRANSLITERATION_BN},
      {"__MSG_TRANSLITERATION_EL__", IDS_IME_NAME_TRANSLITERATION_EL},
      {"__MSG_TRANSLITERATION_FA__", IDS_IME_NAME_TRANSLITERATION_FA},
      {"__MSG_TRANSLITERATION_GU__", IDS_IME_NAME_TRANSLITERATION_GU},
      {"__MSG_TRANSLITERATION_HE__", IDS_IME_NAME_TRANSLITERATION_HE},
      {"__MSG_TRANSLITERATION_HI__", IDS_IME_NAME_TRANSLITERATION_HI},
      {"__MSG_TRANSLITERATION_KN__", IDS_IME_NAME_TRANSLITERATION_KN},
      {"__MSG_TRANSLITERATION_ML__", IDS_IME_NAME_TRANSLITERATION_ML},
      {"__MSG_TRANSLITERATION_MR__", IDS_IME_NAME_TRANSLITERATION_MR},
      {"__MSG_TRANSLITERATION_NE__", IDS_IME_NAME_TRANSLITERATION_NE},
      {"__MSG_TRANSLITERATION_OR__", IDS_IME_NAME_TRANSLITERATION_OR},
      {"__MSG_TRANSLITERATION_PA__", IDS_IME_NAME_TRANSLITERATION_PA},
      {"__MSG_TRANSLITERATION_SA__", IDS_IME_NAME_TRANSLITERATION_SA},
      {"__MSG_TRANSLITERATION_SR__", IDS_IME_NAME_TRANSLITERATION_SR},
      {"__MSG_TRANSLITERATION_TA__", IDS_IME_NAME_TRANSLITERATION_TA},
      {"__MSG_TRANSLITERATION_TE__", IDS_IME_NAME_TRANSLITERATION_TE},
      {"__MSG_TRANSLITERATION_TI__", IDS_IME_NAME_TRANSLITERATION_TI},
      {"__MSG_TRANSLITERATION_UR__", IDS_IME_NAME_TRANSLITERATION_UR},
};

const char kImePathKeyName[] = "ime_path";

extensions::ComponentLoader* GetComponentLoader() {
  // TODO(skuhne, nkostylev): At this time the only thing which makes sense here
  // is to use the active profile. Nkostylev is working on getting IME settings
  // to work for multi user by collecting all settings from all users. Once that
  // is done we might have to re-visit this decision.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionService* extension_service = extension_system->extension_service();
  return extension_service->component_loader();
}
}  // namespace

ComponentExtensionIMEManagerImpl::ComponentExtensionIMEManagerImpl()
    : weak_ptr_factory_(this) {
  ReadComponentExtensionsInfo(&component_extension_list_);
}

ComponentExtensionIMEManagerImpl::~ComponentExtensionIMEManagerImpl() {
}

std::vector<ComponentExtensionIME> ComponentExtensionIMEManagerImpl::ListIME() {
  return component_extension_list_;
}

bool ComponentExtensionIMEManagerImpl::Load(const std::string& extension_id,
                                            const std::string& manifest,
                                            const base::FilePath& file_path) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionService* extension_service = extension_system->extension_service();
  if (extension_service->GetExtensionById(extension_id, false))
    return false;
  const std::string loaded_extension_id =
      GetComponentLoader()->Add(manifest, file_path);
  DCHECK_EQ(loaded_extension_id, extension_id);
  return true;
}

void ComponentExtensionIMEManagerImpl::Unload(const std::string& extension_id,
                                              const base::FilePath& file_path) {
  // Remove(extension_id) does nothing when the extension has already been
  // removed or not been registered.
  GetComponentLoader()->Remove(extension_id);
}

scoped_ptr<base::DictionaryValue> ComponentExtensionIMEManagerImpl::GetManifest(
    const std::string& manifest_string) {
  std::string error;
  JSONStringValueSerializer serializer(manifest_string);
  scoped_ptr<base::Value> manifest(serializer.Deserialize(NULL, &error));
  if (!manifest.get())
    LOG(ERROR) << "Failed at getting manifest";

  return scoped_ptr<base::DictionaryValue>(
             static_cast<base::DictionaryValue*>(manifest.release())).Pass();
}

// static
bool ComponentExtensionIMEManagerImpl::ReadEngineComponent(
    const ComponentExtensionIME& component_extension,
    const base::DictionaryValue& dict,
    ComponentExtensionEngine* out) {
  DCHECK(out);
  std::string type;
  if (!dict.GetString(extensions::manifest_keys::kType, &type))
    return false;
  if (type != "ime")
    return false;
  if (!dict.GetString(extensions::manifest_keys::kId, &out->engine_id))
    return false;
  if (!dict.GetString(extensions::manifest_keys::kName, &out->display_name))
    return false;

  // Localizes the input method name.
  if (out->display_name.find("__MSG_") == 0) {
    const InputMethodNameMap* map = kInputMethodNameMap;
    size_t map_size = arraysize(kInputMethodNameMap);
    std::string name = StringToUpperASCII(out->display_name);
    const InputMethodNameMap map_key = {name.c_str(), 0};
    const InputMethodNameMap* p =
        std::lower_bound(map, map + map_size, map_key);
    if (p != map + map_size && name == p->message_name)
      out->display_name = l10n_util::GetStringUTF8(p->resource_id);
  }
  DCHECK(out->display_name.find("__MSG_") == std::string::npos);

  std::set<std::string> languages;
  const base::Value* language_value = NULL;
  if (dict.Get(extensions::manifest_keys::kLanguage, &language_value)) {
    if (language_value->GetType() == base::Value::TYPE_STRING) {
      std::string language_str;
      language_value->GetAsString(&language_str);
      languages.insert(language_str);
    } else if (language_value->GetType() == base::Value::TYPE_LIST) {
      const base::ListValue* language_list = NULL;
      language_value->GetAsList(&language_list);
      for (size_t j = 0; j < language_list->GetSize(); ++j) {
        std::string language_str;
        if (language_list->GetString(j, &language_str))
          languages.insert(language_str);
      }
    }
  }
  DCHECK(!languages.empty());
  out->language_codes.assign(languages.begin(), languages.end());

  const base::ListValue* layouts = NULL;
  if (!dict.GetList(extensions::manifest_keys::kLayouts, &layouts))
    return false;

  for (size_t i = 0; i < layouts->GetSize(); ++i) {
    std::string buffer;
    if (layouts->GetString(i, &buffer))
      out->layouts.push_back(buffer);
  }

  std::string url_string;
  if (dict.GetString(extensions::manifest_keys::kInputView,
                     &url_string)) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(
            component_extension.id),
        url_string);
    if (!url.is_valid())
      return false;
    out->input_view_url = url;
  }

  if (dict.GetString(extensions::manifest_keys::kOptionsPage,
                     &url_string)) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(
            component_extension.id),
        url_string);
    if (!url.is_valid())
      return false;
    out->options_page_url = url;
  } else {
    // Fallback to extension level options page.
    out->options_page_url = component_extension.options_page_url;
  }

  return true;
}

// static
bool ComponentExtensionIMEManagerImpl::ReadExtensionInfo(
    const base::DictionaryValue& manifest,
    const std::string& extension_id,
    ComponentExtensionIME* out) {
  if (!manifest.GetString(extensions::manifest_keys::kDescription,
                          &out->description))
    return false;
  std::string path;
  if (manifest.GetString(kImePathKeyName, &path))
    out->path = base::FilePath(path);
  std::string url_string;
  if (manifest.GetString(extensions::manifest_keys::kOptionsPage,
                         &url_string)) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id),
        url_string);
    if (!url.is_valid())
      return false;
    out->options_page_url = url;
  }
  // It's okay to return true on no option page and/or input view page case.
  return true;
}

// static
void ComponentExtensionIMEManagerImpl::ReadComponentExtensionsInfo(
    std::vector<ComponentExtensionIME>* out_imes) {
  DCHECK(out_imes);
  for (size_t i = 0; i < arraysize(whitelisted_component_extension); ++i) {
    ComponentExtensionIME component_ime;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    component_ime.manifest =
        rb.GetRawDataResource(
               whitelisted_component_extension[i].manifest_resource_id)
            .as_string();
    if (component_ime.manifest.empty())
      continue;

    scoped_ptr<base::DictionaryValue> manifest =
        GetManifest(component_ime.manifest);
    if (!manifest.get())
      continue;

    if (!ReadExtensionInfo(*manifest.get(),
                           whitelisted_component_extension[i].id,
                           &component_ime))
      continue;
    component_ime.id = whitelisted_component_extension[i].id;

    if (!component_ime.path.IsAbsolute()) {
      base::FilePath resources_path;
      if (!PathService::Get(chrome::DIR_RESOURCES, &resources_path))
        NOTREACHED();
      component_ime.path = resources_path.Append(component_ime.path);
    }

    const base::ListValue* component_list;
    if (!manifest->GetList(extensions::manifest_keys::kInputComponents,
                           &component_list))
      continue;

    for (size_t i = 0; i < component_list->GetSize(); ++i) {
      const base::DictionaryValue* dictionary;
      if (!component_list->GetDictionary(i, &dictionary))
        continue;

      ComponentExtensionEngine engine;
      ReadEngineComponent(component_ime, *dictionary, &engine);
      component_ime.engines.push_back(engine);
    }
    out_imes->push_back(component_ime);
  }
}

}  // namespace chromeos
