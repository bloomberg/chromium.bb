// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/json/json_value_serializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OFFICIAL_BUILD)
#include "chrome/browser/defaults.h"
#endif

namespace {

typedef std::list<std::pair<FilePath::StringType, int> >
    ComponentExtensionList;

#if defined(FILE_MANAGER_EXTENSION)
void AddFileManagerExtension(ComponentExtensionList* component_extensions) {
#ifndef NDEBUG
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kFileManagerExtensionPath)) {
    FilePath filemgr_extension_path =
        command_line->GetSwitchValuePath(switches::kFileManagerExtensionPath);
    component_extensions->push_back(std::make_pair(
        filemgr_extension_path.value(),
        IDR_FILEMANAGER_MANIFEST));
    return;
  }
#endif  // NDEBUG
  component_extensions->push_back(std::make_pair(
      FILE_PATH_LITERAL("file_manager"),
      IDR_FILEMANAGER_MANIFEST));
}
#endif  // defined(FILE_MANAGER_EXTENSION)

}  // namespace

namespace extensions {

bool ComponentLoader::ComponentExtensionInfo::Equals(
    const ComponentExtensionInfo& other) const {
  return other.manifest == manifest && other.root_directory == root_directory;
}

ComponentLoader::ComponentLoader(ExtensionService* extension_service)
    : extension_service_(extension_service) {
}

ComponentLoader::~ComponentLoader() {
}

void ComponentLoader::LoadAll() {
  for (RegisteredComponentExtensions::iterator it =
           component_extensions_.begin();
       it != component_extensions_.end(); ++it) {
    Load(*it);
  }
}

const Extension* ComponentLoader::Add(
    const std::string& manifest, const FilePath& root_directory) {
  ComponentExtensionInfo info(manifest, root_directory);
  Register(info);
  if (extension_service_->is_ready())
    return Load(info);
  return NULL;
}

const Extension* ComponentLoader::Load(const ComponentExtensionInfo& info) {
  JSONStringValueSerializer serializer(info.manifest);
  scoped_ptr<Value> manifest(serializer.Deserialize(NULL, NULL));
  if (!manifest.get()) {
    LOG(ERROR) << "Failed to parse manifest for extension";
    return NULL;
  }

  int flags = Extension::REQUIRE_KEY;
  if (Extension::ShouldDoStrictErrorChecking(Extension::COMPONENT))
    flags |= Extension::STRICT_ERROR_CHECKS;
  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      info.root_directory,
      Extension::COMPONENT,
      *static_cast<DictionaryValue*>(manifest.get()),
      flags,
      &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return NULL;
  }
  extension_service_->AddExtension(extension);
  return extension;
}

void ComponentLoader::Remove(const std::string& manifest_str) {
  // Unload the extension.
  JSONStringValueSerializer serializer(manifest_str);
  scoped_ptr<Value> manifest(serializer.Deserialize(NULL, NULL));
  if (!manifest.get()) {
    LOG(ERROR) << "Failed to parse manifest for extension";
    return;
  }
  std::string public_key;
  std::string public_key_bytes;
  std::string id;
  if (!static_cast<DictionaryValue*>(manifest.get())->
      GetString(extension_manifest_keys::kPublicKey, &public_key) ||
      !Extension::ParsePEMKeyBytes(public_key, &public_key_bytes) ||
      !Extension::GenerateId(public_key_bytes, &id)) {
    LOG(ERROR) << "Failed to get extension id";
    return;
  }
  extension_service_->
      UnloadExtension(id, extension_misc::UNLOAD_REASON_DISABLE);

  // Unregister the extension.
  RegisteredComponentExtensions new_component_extensions;
  for (RegisteredComponentExtensions::iterator it =
           component_extensions_.begin();
       it != component_extensions_.end(); ++it) {
    if (it->manifest != manifest_str)
      new_component_extensions.push_back(*it);
  }
  component_extensions_.swap(new_component_extensions);
}

// We take ComponentExtensionList:
// path, manifest ID => full manifest, absolute path
void ComponentLoader::AddDefaultComponentExtensions() {
  ComponentExtensionList component_extensions;

  // Bookmark manager.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("bookmark_manager"),
      IDR_BOOKMARKS_MANIFEST));

#if defined(FILE_MANAGER_EXTENSION)
  AddFileManagerExtension(&component_extensions);
#endif

#if defined(USE_VIRTUAL_KEYBOARD)
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("keyboard"),
      IDR_KEYBOARD_MANIFEST));
#endif

#if defined(OS_CHROMEOS)
    component_extensions.push_back(std::make_pair(
        FILE_PATH_LITERAL("/usr/share/chromeos-assets/mobile"),
        IDR_MOBILE_MANIFEST));

    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kAuthExtensionPath)) {
      FilePath auth_extension_path =
          command_line->GetSwitchValuePath(switches::kAuthExtensionPath);
      component_extensions.push_back(std::make_pair(
          auth_extension_path.value(),
          IDR_GAIA_TEST_AUTH_MANIFEST));
    } else {
      component_extensions.push_back(std::make_pair(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/gaia_auth"),
          IDR_GAIA_AUTH_MANIFEST));
    }

#if defined(OFFICIAL_BUILD)
    if (browser_defaults::enable_help_app) {
      component_extensions.push_back(std::make_pair(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/helpapp"),
          IDR_HELP_MANIFEST));
    }
#endif
#endif

  // Web Store.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("web_store"),
      IDR_WEBSTORE_MANIFEST));

#if !defined(OS_CHROMEOS)
  // Cloud Print component app. Not required on Chrome OS.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("cloud_print"),
      IDR_CLOUDPRINT_MANIFEST));
#endif  // !defined(OS_CHROMEOS)

  for (ComponentExtensionList::iterator iter = component_extensions.begin();
    iter != component_extensions.end(); ++iter) {
    FilePath path(iter->first);
    if (!path.IsAbsolute()) {
      if (PathService::Get(chrome::DIR_RESOURCES, &path)) {
        path = path.Append(iter->first);
      } else {
        NOTREACHED();
      }
    }

    std::string manifest =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            iter->second).as_string();
    Add(manifest, path);
  }

#if defined(OS_CHROMEOS)
  // Register access extensions only if accessibility is enabled.
  if (g_browser_process->local_state()->
      GetBoolean(prefs::kAccessibilityEnabled)) {
    FilePath path = FilePath(extension_misc::kAccessExtensionPath)
        .AppendASCII(extension_misc::kChromeVoxDirectoryName);
    std::string manifest =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_CHROMEVOX_MANIFEST).as_string();
    Add(manifest, path);
  }
#endif
}

}  // namespace extensions
