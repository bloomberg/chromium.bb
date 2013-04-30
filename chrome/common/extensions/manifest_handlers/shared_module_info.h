// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_SHARED_MODULE_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_SHARED_MODULE_INFO_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

class SharedModuleInfo : public Extension::ManifestData {
 public:
  SharedModuleInfo();
  virtual ~SharedModuleInfo();

  bool Parse(const Extension* extension, string16* error);

  struct ImportInfo {
    std::string extension_id;
    std::string minimum_version;
  };

  // Utility functions.
  static void ParseImportedPath(const std::string& path,
                                std::string* import_id,
                                std::string* import_relative_path);
  static bool IsImportedPath(const std::string& path);

  // Functions relating to exporting resources.
  static bool IsSharedModule(const Extension* extension);
  static bool IsExportAllowed(const Extension* extension,
                              const std::string& relative_path);

  // Functions relating to importing resources.
  static bool ImportsExtensionById(const Extension* extension,
                                   const std::string& other_id);
  static bool ImportsModules(const Extension* extension);
  static const std::vector<ImportInfo>& GetImports(const Extension* extension);

 private:
  // This extension exports the following resources to other extensions.
  URLPatternSet exported_set_;

  // Optional list of module imports of other extensions.
  std::vector<ImportInfo> imports_;
};

// Parses all import/export keys in the manifest.
class SharedModuleHandler : public ManifestHandler {
 public:
  SharedModuleHandler();
  virtual ~SharedModuleHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_SHARED_MODULE_INFO_H_
