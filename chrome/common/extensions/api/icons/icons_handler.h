// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_ICONS_ICONS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_ICONS_ICONS_HANDLER_H_

#include <string>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "extensions/common/extension_resource.h"

namespace extensions {

struct IconsInfo : public Extension::ManifestData {
  // Max size (both dimensions) for browser and page actions.
  static const int kPageActionIconMaxSize;
  static const int kBrowserActionIconMaxSize;

  // The icons for the extension.
  ExtensionIconSet icons;

  // Return the icon set for the given |extension|.
  static const ExtensionIconSet& GetIcons(const Extension* extension);

  // Given an extension, icon size, and match type, read a valid icon if present
  // and decode it into |result|. In the browser process, this will DCHECK if
  // not called on the file thread. To easily load extension images on the UI
  // thread, see ImageLoader.
  static void DecodeIcon(const Extension* extension,
                         int icon_size,
                         ExtensionIconSet::MatchType match_type,
                         scoped_ptr<SkBitmap>* result);

  // Given an extension and icon size, read it if present and decode it into
  // |result|. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoader.
  static void DecodeIcon(const Extension* extension,
                         int icon_size,
                         scoped_ptr<SkBitmap>* result);

  // Given an icon_path and icon size, read it if present and decode it into
  // |result|. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoader.
  static void DecodeIconFromPath(const base::FilePath& icon_path,
                                 int icon_size,
                                 scoped_ptr<SkBitmap>* result);

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

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_ICONS_ICONS_HANDLER_H_
