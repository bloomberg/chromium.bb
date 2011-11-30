// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/json/json_value_serializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OFFICIAL_BUILD)
#include "chrome/browser/defaults.h"
#endif

namespace extensions {

ComponentLoader::ComponentLoader(ExtensionServiceInterface* extension_service,
                                 PrefService* prefs,
                                 PrefService* local_state)
    : prefs_(prefs),
      local_state_(local_state),
      extension_service_(extension_service) {
  pref_change_registrar_.Init(prefs);

  // This pref is set by policy. We have to watch it for change because on
  // ChromeOS, policy isn't loaded until after the browser process is started.
  pref_change_registrar_.Add(prefs::kEnterpriseWebStoreURL, this);
}

ComponentLoader::~ComponentLoader() {
  ClearAllRegistered();
}

void ComponentLoader::LoadAll() {
  for (RegisteredComponentExtensions::iterator it =
          component_extensions_.begin();
      it != component_extensions_.end(); ++it) {
    Load(*it);
  }
}

DictionaryValue* ComponentLoader::ParseManifest(
    const std::string& manifest_contents) const {
  JSONStringValueSerializer serializer(manifest_contents);
  scoped_ptr<Value> manifest(serializer.Deserialize(NULL, NULL));

  if (!manifest.get() || !manifest->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Failed to parse extension manifest.";
    return NULL;
  }
  // Transfer ownership to the caller.
  return static_cast<DictionaryValue*>(manifest.release());
}

void ComponentLoader::ClearAllRegistered() {
  for (RegisteredComponentExtensions::iterator it =
          component_extensions_.begin();
      it != component_extensions_.end(); ++it) {
      delete it->manifest;
  }

  component_extensions_.clear();
}

const Extension* ComponentLoader::Add(
    int manifest_resource_id,
    const FilePath& root_directory) {
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();
  return Add(manifest_contents, root_directory);
}

const Extension* ComponentLoader::Add(
    std::string& manifest_contents,
    const FilePath& root_directory) {
  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  DictionaryValue* manifest = ParseManifest(manifest_contents);
  if (manifest)
    return Add(manifest, root_directory);
  return NULL;
}

const Extension* ComponentLoader::Add(
    const DictionaryValue* parsed_manifest,
    const FilePath& root_directory) {
  // Get the absolute path to the extension.
  FilePath absolute_path(root_directory);
  if (!absolute_path.IsAbsolute()) {
    if (PathService::Get(chrome::DIR_RESOURCES, &absolute_path)) {
      absolute_path = absolute_path.Append(root_directory);
    } else {
      NOTREACHED();
    }
  }

  ComponentExtensionInfo info(parsed_manifest, absolute_path);
  component_extensions_.push_back(info);
  if (extension_service_->is_ready())
    return Load(info);
  return NULL;
}

const Extension* ComponentLoader::Load(const ComponentExtensionInfo& info) {
  int flags = Extension::REQUIRE_KEY;
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  if (Extension::ShouldDoStrictErrorChecking(Extension::COMPONENT))
    flags |= Extension::STRICT_ERROR_CHECKS;
  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      info.root_directory,
      Extension::COMPONENT,
      *info.manifest,
      flags,
      &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return NULL;
  }
  extension_service_->AddExtension(extension);
  return extension;
}

void ComponentLoader::Remove(const FilePath& root_directory) {
  // Find the ComponentExtensionInfo for the extension.
  RegisteredComponentExtensions::iterator it = component_extensions_.begin();
  for (; it != component_extensions_.end(); ++it) {
    if (it->root_directory == root_directory)
      break;
  }
  // If the extension is not in the list, there's nothing to do.
  if (it == component_extensions_.end())
    return;

  // The list owns the dictionary, so it must be deleted after removal.
  scoped_ptr<const DictionaryValue> manifest(it->manifest);

  // Remove the extension from the list of registered extensions.
  *it = component_extensions_.back();
  component_extensions_.pop_back();

  // Determine the extension id and unload the extension.
  std::string public_key;
  std::string public_key_bytes;
  std::string id;
  if (!manifest->GetString(
          extension_manifest_keys::kPublicKey, &public_key) ||
      !Extension::ParsePEMKeyBytes(public_key, &public_key_bytes) ||
      !Extension::GenerateId(public_key_bytes, &id)) {
    LOG(ERROR) << "Failed to get extension id";
    return;
  }
  extension_service_->
      UnloadExtension(id, extension_misc::UNLOAD_REASON_DISABLE);
}

void ComponentLoader::AddFileManagerExtension() {
#if defined(FILE_MANAGER_EXTENSION)
#ifndef NDEBUG
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kFileManagerExtensionPath)) {
    FilePath filemgr_extension_path(
        command_line->GetSwitchValuePath(switches::kFileManagerExtensionPath));
    Add(IDR_FILEMANAGER_MANIFEST, filemgr_extension_path);
    return;
  }
#endif  // NDEBUG
  Add(IDR_FILEMANAGER_MANIFEST, FilePath(FILE_PATH_LITERAL("file_manager")));
#endif  // defined(FILE_MANAGER_EXTENSION)
}

void ComponentLoader::AddOrReloadEnterpriseWebStore() {
  FilePath path(FILE_PATH_LITERAL("enterprise_web_store"));

  // Remove the extension if it was already loaded.
  Remove(path);

  std::string enterprise_webstore_url =
      prefs_->GetString(prefs::kEnterpriseWebStoreURL);

  // Load the extension only if the URL preference is set.
  if (!enterprise_webstore_url.empty()) {
    std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ENTERPRISE_WEBSTORE_MANIFEST).as_string();

    // The manifest is missing some values that are provided by policy.
    DictionaryValue* manifest = ParseManifest(manifest_contents);
    if (manifest) {
      std::string name = prefs_->GetString(prefs::kEnterpriseWebStoreName);
      manifest->SetString("app.launch.web_url", enterprise_webstore_url);
      manifest->SetString("name", name);
      Add(manifest, path);
    }
  }
}

void ComponentLoader::AddDefaultComponentExtensions() {
  Add(IDR_BOOKMARKS_MANIFEST, FilePath(FILE_PATH_LITERAL("bookmark_manager")));

#if defined(FILE_MANAGER_EXTENSION)
  AddFileManagerExtension();
#endif

#if defined(USE_VIRTUAL_KEYBOARD)
  Add(IDR_KEYBOARD_MANIFEST, FilePath(FILE_PATH_LITERAL("keyboard")));
#endif

#if defined(OS_CHROMEOS)
    Add(IDR_MOBILE_MANIFEST,
        FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/mobile")));

    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kAuthExtensionPath)) {
      FilePath auth_extension_path =
          command_line->GetSwitchValuePath(switches::kAuthExtensionPath);
      Add(IDR_GAIA_TEST_AUTH_MANIFEST, auth_extension_path);
    } else {
      Add(IDR_GAIA_AUTH_MANIFEST,
          FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/gaia_auth")));
    }

#if defined(OFFICIAL_BUILD)
    if (browser_defaults::enable_help_app) {
      Add(IDR_HELP_MANIFEST,
          FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/helpapp")));
    }
#endif
#endif  // !defined(OS_CHROMEOS)

  Add(IDR_WEBSTORE_MANIFEST, FilePath(FILE_PATH_LITERAL("web_store")));

#if !defined(OS_CHROMEOS)
  // Cloud Print component app. Not required on Chrome OS.
  Add(IDR_CLOUDPRINT_MANIFEST, FilePath(FILE_PATH_LITERAL("cloud_print")));
#endif

#if defined(OS_CHROMEOS)
  // Register access extensions only if accessibility is enabled.
  if (local_state_->GetBoolean(prefs::kAccessibilityEnabled)) {
    FilePath path = FilePath(extension_misc::kAccessExtensionPath)
        .AppendASCII(extension_misc::kChromeVoxDirectoryName);
    Add(IDR_CHROMEVOX_MANIFEST, path);
  }
#endif

  // If a URL for the enterprise webstore has been specified, load the
  // component extension. This extension might also be loaded later, because
  // it is specified by policy, and on ChromeOS policies are loaded after
  // the browser process has started.
  AddOrReloadEnterpriseWebStore();
}

void ComponentLoader::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    const std::string* name =
        content::Details<const std::string>(details).ptr();
    if (*name == prefs::kEnterpriseWebStoreURL)
      AddOrReloadEnterpriseWebStore();
    else
      NOTREACHED();
  } else {
    NOTREACHED();
  }
}

// static
void ComponentLoader::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kEnterpriseWebStoreURL,
                            std::string() /* default_value */,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kEnterpriseWebStoreName,
                            std::string() /* default_value */,
                            PrefService::UNSYNCABLE_PREF);
}

}  // namespace extensions
