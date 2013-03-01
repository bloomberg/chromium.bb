// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_WEB_ACCESSIBLE_RESOURCES_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_WEB_ACCESSIBLE_RESOURCES_HANDLER_H_

#include <string>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

// A structure to hold the web accessible extension resources
// that may be specified in the manifest of an extension using
// "web_accessible_resources" key.
struct WebAccessibleResourcesInfo : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  WebAccessibleResourcesInfo();
  virtual ~WebAccessibleResourcesInfo();

  // Returns true if the specified resource is web accessible.
  static bool IsResourceWebAccessible(const Extension* extension,
                                      const std::string& relative_path);

  // Returns true when 'web_accessible_resources' are defined for the extension.
  static bool HasWebAccessibleResources(const Extension* extension);

  // Optional list of web accessible extension resources.
  URLPatternSet web_accessible_resources_;
};

// Parses the "web_accessible_resources" manifest key.
class WebAccessibleResourcesHandler : public ManifestHandler {
 public:
  WebAccessibleResourcesHandler();
  virtual ~WebAccessibleResourcesHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebAccessibleResourcesHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_WEB_ACCESSIBLE_RESOURCES_HANDLER_H_
