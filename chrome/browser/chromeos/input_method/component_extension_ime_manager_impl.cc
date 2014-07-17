// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chromeos/ime/extension_ime_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

struct WhitelistedComponentExtensionIME {
  const char* id;
  const char* path;
} whitelisted_component_extension[] = {
  {
    // ChromeOS Hangul Input.
    extension_ime_util::kHangulExtensionId,
    "/usr/share/chromeos-assets/input_methods/hangul",
  },
#if defined(OFFICIAL_BUILD)
  {
    // Official Google XKB Input.
    extension_ime_util::kXkbExtensionId,
    "/usr/share/chromeos-assets/input_methods/google_xkb",
  },
  {
    // Google input tools.
    extension_ime_util::kT13nExtensionId,
    "/usr/share/chromeos-assets/input_methods/input_tools",
  },
#else
  {
    // Open-sourced ChromeOS xkb extension.
    extension_ime_util::kXkbExtensionId,
    "/usr/share/chromeos-assets/input_methods/xkb",
  },
  {
    // Open-sourced ChromeOS Keyboards extension.
    extension_ime_util::kM17nExtensionId,
    "/usr/share/chromeos-assets/input_methods/keyboard_layouts",
  },
  {
    // Open-sourced Pinyin Chinese Input Method.
    extension_ime_util::kChinesePinyinExtensionId,
    "/usr/share/chromeos-assets/input_methods/pinyin",
  },
  {
    // Open-sourced Zhuyin Chinese Input Method.
    extension_ime_util::kChineseZhuyinExtensionId,
    "/usr/share/chromeos-assets/input_methods/zhuyin",
  },
  {
    // Open-sourced Cangjie Chinese Input Method.
    extension_ime_util::kChineseCangjieExtensionId,
    "/usr/share/chromeos-assets/input_methods/cangjie",
  },
  {
    // Japanese Mozc Input.
    extension_ime_util::kMozcExtensionId,
    "/usr/share/chromeos-assets/input_methods/nacl_mozc",
  },
#endif
  {
    // Braille hardware keyboard IME that works together with ChromeVox.
    extension_misc::kBrailleImeExtensionId,
    extension_misc::kBrailleImeExtensionPath,
  },
};

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
    : is_initialized_(false),
      weak_ptr_factory_(this) {
}

ComponentExtensionIMEManagerImpl::~ComponentExtensionIMEManagerImpl() {
}

std::vector<ComponentExtensionIME> ComponentExtensionIMEManagerImpl::ListIME() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return component_extension_list_;
}

bool ComponentExtensionIMEManagerImpl::Load(const std::string& extension_id,
                                            const std::string& manifest,
                                            const base::FilePath& file_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DCHECK(thread_checker_.CalledOnValidThread());
  // Remove(extension_id) does nothing when the extension has already been
  // removed or not been registered.
  GetComponentLoader()->Remove(extension_id);
}

scoped_ptr<base::DictionaryValue> ComponentExtensionIMEManagerImpl::GetManifest(
    const base::FilePath& file_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  std::string error;
  scoped_ptr<base::DictionaryValue> manifest(
      extensions::file_util::LoadManifest(file_path, &error));
  if (!manifest.get())
    LOG(ERROR) << "Failed at getting manifest";
  if (!extension_l10n_util::LocalizeExtension(file_path,
                                              manifest.get(),
                                              &error)) {
    LOG(ERROR) << "Localization failed for: " << file_path.value()
               << " Error: " << error;
  }
  return manifest.Pass();
}

void ComponentExtensionIMEManagerImpl::InitializeAsync(
    const base::Closure& callback) {
  DCHECK(!is_initialized_);
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<ComponentExtensionIME>* component_extension_ime_list
      = new std::vector<ComponentExtensionIME>;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ComponentExtensionIMEManagerImpl::ReadComponentExtensionsInfo,
                 base::Unretained(component_extension_ime_list)),
      base::Bind(
          &ComponentExtensionIMEManagerImpl::OnReadComponentExtensionsInfo,
          weak_ptr_factory_.GetWeakPtr(),
          base::Owned(component_extension_ime_list),
          callback));
}

bool ComponentExtensionIMEManagerImpl::IsInitialized() {
  return is_initialized_;
}

// static
bool ComponentExtensionIMEManagerImpl::ReadEngineComponent(
    const ComponentExtensionIME& component_extension,
    const base::DictionaryValue& dict,
    ComponentExtensionEngine* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (!manifest.GetString(extensions::manifest_keys::kDescription,
                          &out->description))
    return false;
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(out_imes);
  for (size_t i = 0; i < arraysize(whitelisted_component_extension); ++i) {
    ComponentExtensionIME component_ime;
    component_ime.path = base::FilePath(
        whitelisted_component_extension[i].path);

    if (!component_ime.path.IsAbsolute()) {
      base::FilePath resources_path;
      if (!PathService::Get(chrome::DIR_RESOURCES, &resources_path))
        NOTREACHED();
      component_ime.path = resources_path.Append(component_ime.path);
    }
    const base::FilePath manifest_path =
        component_ime.path.Append("manifest.json");

    if (!base::PathExists(component_ime.path) ||
        !base::PathExists(manifest_path))
      continue;

    if (!base::ReadFileToString(manifest_path, &component_ime.manifest))
      continue;

    scoped_ptr<base::DictionaryValue> manifest =
        GetManifest(component_ime.path);
    if (!manifest.get())
      continue;

    if (!ReadExtensionInfo(*manifest.get(),
                           whitelisted_component_extension[i].id,
                           &component_ime))
      continue;
    component_ime.id = whitelisted_component_extension[i].id;

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

void ComponentExtensionIMEManagerImpl::OnReadComponentExtensionsInfo(
    std::vector<ComponentExtensionIME>* result,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(result);
  component_extension_list_ = *result;
  is_initialized_ = true;
  callback.Run();
}

}  // namespace chromeos
