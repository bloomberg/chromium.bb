// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/values.h"

class ExtensionServiceInterface;
class PrefService;
class PrefRegistrySyncable;

namespace extensions {

class Extension;

// For registering, loading, and unloading component extensions.
class ComponentLoader {
 public:
  ComponentLoader(ExtensionServiceInterface* extension_service,
                  PrefService* prefs,
                  PrefService* local_state);
  virtual ~ComponentLoader();

  size_t registered_extensions_count() const {
    return component_extensions_.size();
  }

  // Creates and loads all registered component extensions.
  void LoadAll();

  // Clear the list of all registered extensions and unloads them from the
  // extension service.
  void RemoveAll();

  // Registers and possibly loads a component extension. If ExtensionService
  // has been initialized, the extension is loaded; otherwise, the load is
  // deferred until LoadAll is called. The ID of the added extension is
  // returned.
  //
  // Component extension manifests must contain a "key" property with a unique
  // public key, serialized in base64. You can create a suitable value with the
  // following commands on a unixy system:
  //
  //   ssh-keygen -t rsa -b 1024 -N '' -f /tmp/key.pem
  //   openssl rsa -pubout -outform DER < /tmp/key.pem 2>/dev/null | base64 -w 0
  std::string Add(const std::string& manifest_contents,
                  const base::FilePath& root_directory);

  // Convenience method for registering a component extension by resource id.
  std::string Add(int manifest_resource_id,
                  const base::FilePath& root_directory);

  // Loads a component extension from file system. Replaces previously added
  // extension with the same ID.
  std::string AddOrReplace(const base::FilePath& path);

  // Returns true if an extension with the specified id has been added.
  bool Exists(const std::string& id) const;

  // Unloads a component extension and removes it from the list of component
  // extensions to be loaded.
  void Remove(const base::FilePath& root_directory);
  void Remove(const std::string& id);

  // Call this during test setup to load component extensions that have
  // background pages for testing, which could otherwise interfere with tests.
  static void EnableBackgroundExtensionsForTesting();

  // Adds the default component extensions. If |skip_session_components|
  // the loader will skip loading component extensions that weren't supposed to
  // be loaded unless we are in signed user session (ChromeOS). For all other
  // platforms this |skip_session_components| is expected to be unset.
  void AddDefaultComponentExtensions(bool skip_session_components);

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Parse the given JSON manifest. Returns NULL if it cannot be parsed, or if
  // if the result is not a DictionaryValue.
  DictionaryValue* ParseManifest(const std::string& manifest_contents) const;

  // Clear the list of registered extensions.
  void ClearAllRegistered();

  // Reloads a registered component extension.
  void Reload(const std::string& extension_id);

 private:
  // Information about a registered component extension.
  struct ComponentExtensionInfo {
    ComponentExtensionInfo(const DictionaryValue* manifest,
                           const base::FilePath& root_directory);

    // The parsed contents of the extensions's manifest file.
    const DictionaryValue* manifest;

    // Directory where the extension is stored.
    base::FilePath root_directory;

    // The component extension's ID.
    std::string extension_id;
  };

  std::string Add(const DictionaryValue* parsed_manifest,
                  const base::FilePath& root_directory);

  // Loads a registered component extension.
  void Load(const ComponentExtensionInfo& info);

  void AddDefaultComponentExtensionsWithBackgroundPages(
      bool skip_session_components);
  void AddFileManagerExtension();
  void AddImageLoaderExtension();

#if defined(OS_CHROMEOS)
  void AddGaiaAuthExtension();
#endif

  // Add the enterprise webstore extension, or reload it if already loaded.
  void AddOrReloadEnterpriseWebStore();

  void AddChromeApp();

  // Unloads |component| from the memory.
  void UnloadComponent(ComponentExtensionInfo* component);

  PrefService* profile_prefs_;
  PrefService* local_state_;

  ExtensionServiceInterface* extension_service_;

  // List of registered component extensions (see Manifest::Location).
  typedef std::vector<ComponentExtensionInfo> RegisteredComponentExtensions;
  RegisteredComponentExtensions component_extensions_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ComponentLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
