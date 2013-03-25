// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"

#include "base/logging.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

struct WhitelistedComponentExtensionIME {
  const char* id;
  const char* path;
} whitelisted_component_extension[] = {
  {
    "fpfbhcjppmaeaijcidgiibchfbnhbelj",
    "/usr/share/chromeos-assets/input_methods/nacl_mozc",
  },
};

extensions::ComponentLoader* GetComponentLoader() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionService* extension_service = extension_system->extension_service();
  return extension_service->component_loader();
}
}  // namespace

ComponentExtentionIMEManagerImpl::ComponentExtentionIMEManagerImpl()
    : is_initialized_(false),
      weak_ptr_factory_(this) {
}

ComponentExtentionIMEManagerImpl::~ComponentExtentionIMEManagerImpl() {
}

std::vector<ComponentExtensionIME> ComponentExtentionIMEManagerImpl::ListIME() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return component_extension_list_;
}

bool ComponentExtentionIMEManagerImpl::Load(const std::string& extension_id,
                                            const base::FilePath& file_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (loaded_extension_id_.find(extension_id) != loaded_extension_id_.end())
    return false;
  const std::string loaded_extension_id =
      GetComponentLoader()->AddOrReplace(file_path);
  DCHECK_EQ(loaded_extension_id, extension_id);
  loaded_extension_id_.insert(extension_id);
  return true;
}

bool ComponentExtentionIMEManagerImpl::Unload(const std::string& extension_id,
                                              const base::FilePath& file_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (loaded_extension_id_.find(extension_id) == loaded_extension_id_.end())
    return false;
  GetComponentLoader()->Remove(extension_id);
  loaded_extension_id_.erase(extension_id);
  return true;
}

scoped_ptr<DictionaryValue> ComponentExtentionIMEManagerImpl::GetManifest(
    const base::FilePath& file_path) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      extension_file_util::LoadManifest(file_path, &error));
  if (!manifest.get())
    LOG(ERROR) << "Failed at getting manifest";
  if (!extension_l10n_util::LocalizeExtension(file_path,
                                              manifest.get(),
                                              &error))
    LOG(ERROR) << "Localization failed";

  return manifest.Pass();
}

void ComponentExtentionIMEManagerImpl::Initialize(
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
    const base::Closure& callback) {
  DCHECK(!is_initialized_);
  std::vector<ComponentExtensionIME>* component_extension_ime_list
      = new std::vector<ComponentExtensionIME>;
  file_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ComponentExtentionIMEManagerImpl::ReadComponentExtensionsInfo,
                 base::Unretained(component_extension_ime_list)),
      base::Bind(
          &ComponentExtentionIMEManagerImpl::OnReadComponentExtensionsInfo,
          weak_ptr_factory_.GetWeakPtr(),
          base::Owned(component_extension_ime_list),
          callback));
}

bool ComponentExtentionIMEManagerImpl::IsInitialized() {
  return is_initialized_;
}

// static
bool ComponentExtentionIMEManagerImpl::ReadEngineComponent(
    const DictionaryValue& dict,
    IBusComponent::EngineDescription* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(out);
  std::string type;
  if (!dict.GetString(extension_manifest_keys::kType, &type))
    return false;
  if (type != "ime")
    return false;
  if (!dict.GetString(extension_manifest_keys::kId, &out->engine_id))
    return false;
  if (!dict.GetString(extension_manifest_keys::kName, &out->display_name))
    return false;
  if (!dict.GetString(extension_manifest_keys::kLanguage, &out->language_code))
    return false;

  const ListValue* layouts = NULL;
  if (!dict.GetList(extension_manifest_keys::kLayouts, &layouts))
    return false;

  if (layouts->GetSize() > 0) {
    if (!layouts->GetString(0, &out->layout))
      return false;
  }
  return true;
}

// static
bool ComponentExtentionIMEManagerImpl::ReadExtensionInfo(
    const DictionaryValue& manifest,
    ComponentExtensionIME* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (!manifest.GetString(extension_manifest_keys::kDescription,
                          &out->description))
    return false;
  // TODO(nona): option page handling.
  return true;
}

// static
void ComponentExtentionIMEManagerImpl::ReadComponentExtensionsInfo(
    std::vector<ComponentExtensionIME>* out_imes) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(out_imes);
  for (size_t i = 0; i < arraysize(whitelisted_component_extension); ++i) {
    const base::FilePath extension_path = base::FilePath(
        whitelisted_component_extension[i].path);

    scoped_ptr<DictionaryValue> manifest = GetManifest(extension_path);
    if (!manifest.get())
      continue;

    ComponentExtensionIME component_ime;
    if (!ReadExtensionInfo(*manifest.get(), &component_ime))
      continue;

    const ListValue* component_list;
    if (!manifest->GetList(extension_manifest_keys::kInputComponents,
                           &component_list))
      continue;

    for (size_t i = 0; i < component_list->GetSize(); ++i) {
      const DictionaryValue* dictionary;
      if (!component_list->GetDictionary(i, &dictionary))
        continue;

      IBusComponent::EngineDescription engine;
      ReadEngineComponent(*dictionary, &engine);
      component_ime.engines.push_back(engine);
    }
    out_imes->push_back(component_ime);
  }
}

void ComponentExtentionIMEManagerImpl::OnReadComponentExtensionsInfo(
    std::vector<ComponentExtensionIME>* result,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(result);
  component_extension_list_ = *result;
  is_initialized_ = true;
  callback.Run();
}

}  // namespace chromeos
