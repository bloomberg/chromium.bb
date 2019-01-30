// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_COMPONENT_EXTENSION_RESOURCE_MANAGER_H_
#define EXTENSIONS_BROWSER_COMPONENT_EXTENSION_RESOURCE_MANAGER_H_

#include <string>

#include "ui/base/template_expressions.h"

namespace base {
class FilePath;
}

namespace extensions {

// Information about a bundled component extension resource.
struct ComponentExtensionResourceInfo {
  // The resource's ID.
  int resource_id = 0;

  // Whether the resource is stored gzipped. Note that only serving from the
  // chrome-extensions:// scheme can support gzipped resources. User scripts,
  // injected scripts and images may not be gzipped.
  bool gzipped = false;
};

// This class manages which extension resources actually come from
// the resource bundle.
class ComponentExtensionResourceManager {
 public:
  virtual ~ComponentExtensionResourceManager() {}

  // Checks whether image is a component extension resource. Returns false
  // if a given |resource| does not have a corresponding image in bundled
  // resources. Otherwise fills |resource_id|. This doesn't check if the
  // extension the resource is in is actually a component extension.
  virtual bool IsComponentExtensionResource(
      const base::FilePath& extension_path,
      const base::FilePath& resource_path,
      ComponentExtensionResourceInfo* resource_info) const = 0;

  // Returns the i18n template replacements for a component extension if they
  // exist, or nullptr otherwise. If non-null, the returned value must remain
  // valid for the life of this ComponentExtensionResourceManager.
  virtual const ui::TemplateReplacements* GetTemplateReplacementsForExtension(
      const std::string& extension_id) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_COMPONENT_EXTENSION_RESOURCE_MANAGER_H_
