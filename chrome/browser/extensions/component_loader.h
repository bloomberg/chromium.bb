// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"

class ExtensionServiceInterface;
class PrefService;

namespace extensions {

class Extension;

// For registering, loading, and unloading component extensions.
class ComponentLoader : public content::NotificationObserver {
 public:
  ComponentLoader(ExtensionServiceInterface* extension_service,
                  PrefService* prefs,
                  PrefService* local_state);
  virtual ~ComponentLoader();

  size_t registered_extensions_count() const {
    return component_extensions_.size();
  }

  // Loads any registered component extensions.
  void LoadAll();

  // Registers and possibly loads a component extension. If ExtensionService
  // has been initialized, the extension is loaded; otherwise, the load is
  // deferred until LoadAll is called.
  const Extension* Add(const std::string& manifest_contents,
                       const FilePath& root_directory);

  // Convenience method for registering a component extension by resource id.
  const Extension* Add(int manifest_resource_id,
                       const FilePath& root_directory);

  // Loads a component extension from file system. Replaces previously added
  // extension with the same ID.
  const Extension* AddOrReplace(const FilePath& path);

  // Returns true if an extension with the specified id has been added.
  bool Exists(const std::string& id) const;

  // Unloads a component extension and removes it from the list of component
  // extensions to be loaded.
  void Remove(const FilePath& root_directory);
  void Remove(const std::string& id);

  // Adds the default component extensions.
  //
  // Component extension manifests must contain a 'key' property with a unique
  // public key, serialized in base64. You can create a suitable value with the
  // following commands on a unixy system:
  //
  //   ssh-keygen -t rsa -b 1024 -N '' -f /tmp/key.pem
  //   openssl rsa -pubout -outform DER < /tmp/key.pem 2>/dev/null | base64 -w 0
  void AddDefaultComponentExtensions();

  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  static void RegisterUserPrefs(PrefService* prefs);

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
                           const FilePath& root_directory)
        : manifest(manifest),
          root_directory(root_directory) {
    }

    // The parsed contents of the extensions's manifest file.
    const DictionaryValue* manifest;

    // Directory where the extension is stored.
    FilePath root_directory;
  };

  const Extension* Add(const DictionaryValue* parsed_manifest,
                       const FilePath& root_directory);

  // Loads a registered component extension.
  const Extension* Load(const ComponentExtensionInfo& info);

  void AddFileManagerExtension();

#if defined(OS_CHROMEOS)
  void AddGaiaAuthExtension();
#endif

  // Add the enterprise webstore extension, or reload it if already loaded.
  void AddOrReloadEnterpriseWebStore();

  void AddChromeApp();

  // Determine the extension id.
  static std::string GenerateId(const base::DictionaryValue* manifest);

  PrefService* prefs_;
  PrefService* local_state_;

  ExtensionServiceInterface* extension_service_;

  // List of registered component extensions (see Extension::Location).
  typedef std::vector<ComponentExtensionInfo> RegisteredComponentExtensions;
  RegisteredComponentExtensions component_extensions_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ComponentLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
