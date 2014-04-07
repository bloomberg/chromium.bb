// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_ICONS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_ICONS_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct IconsInfo : public Extension::ManifestData {
  // Max size (both dimensions) for browser and page actions.
  static const int kPageActionIconMaxSize;
  static const int kBrowserActionIconMaxSize;

  // The icons for the extension.
  ExtensionIconSet icons;

  // Return the icon set for the given |extension|.
  static const ExtensionIconSet& GetIcons(const Extension* extension);

  // Returns the default extension/app icon (for extensions or apps that don't
  // have one).
  static const gfx::ImageSkia& GetDefaultExtensionIcon();
  static const gfx::ImageSkia& GetDefaultAppIcon();

  // Get an extension icon as a resource or URL.
  static ExtensionResource GetIconResource(
      const Extension* extension,
      int size,
      ExtensionIconSet::MatchType match_type);
  static GURL GetIconURL(const Extension* extension,
                         int size,
                         ExtensionIconSet::MatchType match_type);
};

// Parses the "icons" manifest key.
class IconsHandler : public ManifestHandler {
 public:
  IconsHandler();
  virtual ~IconsHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_ICONS_HANDLER_H_
