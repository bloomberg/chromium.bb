// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
#pragma once

#include <string>

#include "base/file_path.h"

class Extension;
class ExtensionService;

namespace extensions {

// For registering, loading, and unloading component extensions.
class ComponentLoader {
 public:
  explicit ComponentLoader(ExtensionService* extension_service);
  virtual ~ComponentLoader();

  // Loads any registered component extensions.
  void LoadAll();

  // Loads and registers a component extension. If ExtensionService has been
  // initialized, the extension is loaded; otherwise, the load is deferred
  // until LoadAll is called.
  const Extension* Add(const std::string& manifest,
                       const FilePath& root_directory);

  // Unloads a component extension and removes it from the list of component
  // extensions to be loaded.
  void Remove(const std::string& manifest_str);

  // Adds the default component extensions.
  //
  // Component extension manifests must contain a 'key' property with a unique
  // public key, serialized in base64. You can create a suitable value with the
  // following commands on a unixy system:
  //
  //   ssh-keygen -t rsa -b 1024 -N '' -f /tmp/key.pem
  //   openssl rsa -pubout -outform DER < /tmp/key.pem 2>/dev/null | base64 -w 0
  void AddDefaultComponentExtensions();

 private:
  // Information about a registered component extension.
  struct ComponentExtensionInfo {
    ComponentExtensionInfo(const std::string& manifest,
                           const FilePath& root_directory)
        : manifest(manifest),
          root_directory(root_directory) {
    }

    bool Equals(const ComponentExtensionInfo& other) const;

    // The extension's manifest. This is required for component extensions so
    // that ExtensionService doesn't need to go to disk to load them.
    std::string manifest;

    // Directory where the extension is stored.
    FilePath root_directory;
  };

  // Loads a registered component extension.
  const Extension* Load(const ComponentExtensionInfo& info);

  // Registers an extension to be loaded as a component extension.
  void Register(const ComponentExtensionInfo& info) {
    component_extensions_.push_back(info);
  }

  // List of registered component extensions (see Extension::Location).
  typedef std::vector<ComponentExtensionInfo> RegisteredComponentExtensions;
  RegisteredComponentExtensions component_extensions_;

  ExtensionService* extension_service_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
