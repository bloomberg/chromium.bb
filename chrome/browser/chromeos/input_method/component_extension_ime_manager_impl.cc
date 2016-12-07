// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

struct WhitelistedComponentExtensionIME {
  const char* id;
  int manifest_resource_id;
} whitelisted_component_extension[] = {
#if defined(GOOGLE_CHROME_BUILD)
    {
        // Official Google XKB Input.
        extension_ime_util::kXkbExtensionId, IDR_GOOGLE_XKB_MANIFEST,
    },
    {
        // Google input tools.
        extension_ime_util::kT13nExtensionId, IDR_GOOGLE_INPUT_TOOLS_MANIFEST,
    },
#else
    {
        // Open-sourced ChromeOS xkb extension.
        extension_ime_util::kXkbExtensionId, IDR_XKB_MANIFEST,
    },
    {
        // Open-sourced ChromeOS Keyboards extension.
        extension_ime_util::kM17nExtensionId, IDR_M17N_MANIFEST,
    },
    {
        // Open-sourced Pinyin Chinese Input Method.
        extension_ime_util::kChinesePinyinExtensionId, IDR_PINYIN_MANIFEST,
    },
    {
        // Open-sourced Zhuyin Chinese Input Method.
        extension_ime_util::kChineseZhuyinExtensionId, IDR_ZHUYIN_MANIFEST,
    },
    {
        // Open-sourced Cangjie Chinese Input Method.
        extension_ime_util::kChineseCangjieExtensionId, IDR_CANGJIE_MANIFEST,
    },
    {
        // Open-sourced Japanese Mozc Input.
        extension_ime_util::kMozcExtensionId, IDR_MOZC_MANIFEST,
    },
    {
        // Open-sourced Hangul Input.
        extension_ime_util::kHangulExtensionId, IDR_HANGUL_MANIFEST,
    },
#endif
    {
        // Braille hardware keyboard IME that works together with ChromeVox.
        extension_misc::kBrailleImeExtensionId, IDR_BRAILLE_MANIFEST,
    },
};

const char kImePathKeyName[] = "ime_path";

extensions::ComponentLoader* GetComponentLoader(Profile* profile) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionService* extension_service = extension_system->extension_service();
  return extension_service->component_loader();
}

void DoLoadExtension(Profile* profile,
                     const std::string& extension_id,
                     const std::string& manifest,
                     const base::FilePath& file_path) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionService* extension_service = extension_system->extension_service();
  DCHECK(extension_service);
  if (extension_service->GetExtensionById(extension_id, false))
    return;
  const std::string loaded_extension_id =
      GetComponentLoader(profile)->Add(manifest, file_path);
  // Register IME extension with ExtensionPrefValueMap.
  ExtensionPrefValueMapFactory::GetForBrowserContext(profile)
      ->RegisterExtension(extension_id,
                          base::Time(),  // install_time.
                          true,          // is_enabled.
                          true);         // is_incognito_enabled.
  DCHECK_EQ(loaded_extension_id, extension_id);
}

bool CheckFilePath(const base::FilePath* file_path) {
  return base::PathExists(*file_path);
}

void OnFilePathChecked(Profile* profile,
                       const std::string* extension_id,
                       const std::string* manifest,
                       const base::FilePath* file_path,
                       bool result) {
  if (result) {
    DoLoadExtension(profile, *extension_id, *manifest, *file_path);
  } else {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "IME extension file path does not exist: " << file_path->value();
  }
}

}  // namespace

ComponentExtensionIMEManagerImpl::ComponentExtensionIMEManagerImpl() {
  ReadComponentExtensionsInfo(&component_extension_list_);
}

ComponentExtensionIMEManagerImpl::~ComponentExtensionIMEManagerImpl() {
}

std::vector<ComponentExtensionIME> ComponentExtensionIMEManagerImpl::ListIME() {
  return component_extension_list_;
}

void ComponentExtensionIMEManagerImpl::Load(Profile* profile,
                                            const std::string& extension_id,
                                            const std::string& manifest,
                                            const base::FilePath& file_path) {
  // Check the existence of file path to avoid unnecessary extension loading
  // and InputMethodEngine creation, so that the virtual keyboard web content
  // url won't be override by IME component extensions.
  base::FilePath* copied_file_path = new base::FilePath(file_path);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CheckFilePath,
                 base::Unretained(copied_file_path)),
      base::Bind(&OnFilePathChecked,
                 base::Unretained(profile),
                 base::Owned(new std::string(extension_id)),
                 base::Owned(new std::string(manifest)),
                 base::Owned(copied_file_path)));
}

void ComponentExtensionIMEManagerImpl::Unload(Profile* profile,
                                              const std::string& extension_id,
                                              const base::FilePath& file_path) {
  // Remove(extension_id) does nothing when the extension has already been
  // removed or not been registered.
  GetComponentLoader(profile)->Remove(extension_id);
}

std::unique_ptr<base::DictionaryValue>
ComponentExtensionIMEManagerImpl::GetManifest(
    const std::string& manifest_string) {
  std::string error;
  JSONStringValueDeserializer deserializer(manifest_string);
  std::unique_ptr<base::Value> manifest =
      deserializer.Deserialize(NULL, &error);
  if (!manifest.get())
    LOG(ERROR) << "Failed at getting manifest";

  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(manifest.release()));
}

// static
bool ComponentExtensionIMEManagerImpl::IsIMEExtensionID(const std::string& id) {
  for (size_t i = 0; i < arraysize(whitelisted_component_extension); ++i) {
    if (base::LowerCaseEqualsASCII(id, whitelisted_component_extension[i].id))
      return true;
  }
  return false;
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
  if (!dict.GetString(extensions::manifest_keys::kIndicator, &out->indicator))
    out->indicator = "";

  std::set<std::string> languages;
  const base::Value* language_value = NULL;
  if (dict.Get(extensions::manifest_keys::kLanguage, &language_value)) {
    if (language_value->GetType() == base::Value::Type::STRING) {
      std::string language_str;
      language_value->GetAsString(&language_str);
      languages.insert(language_str);
    } else if (language_value->GetType() == base::Value::Type::LIST) {
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

    std::unique_ptr<base::DictionaryValue> manifest =
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
