// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/icons/icons_handler.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler_helpers.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/image_decoder.h"

namespace keys = extension_manifest_keys;

namespace extensions {

static base::LazyInstance<ExtensionIconSet> g_empty_icon_set =
    LAZY_INSTANCE_INITIALIZER;

const int IconsInfo::kPageActionIconMaxSize = 19;
const int IconsInfo::kBrowserActionIconMaxSize = 19;

// static
const ExtensionIconSet& IconsInfo::GetIcons(const Extension* extension) {
  IconsInfo* info = static_cast<IconsInfo*>(
      extension->GetManifestData(keys::kIcons));
  return info ? info->icons : g_empty_icon_set.Get();
}

// static
void IconsInfo::DecodeIcon(const Extension* extension,
                           int preferred_icon_size,
                           ExtensionIconSet::MatchType match_type,
                           scoped_ptr<SkBitmap>* result) {
  std::string path = GetIcons(extension).Get(preferred_icon_size, match_type);
  int size = GetIcons(extension).GetIconSizeFromPath(path);
  ExtensionResource icon_resource = extension->GetResource(path);
  DecodeIconFromPath(icon_resource.GetFilePath(), size, result);
}

// static
void IconsInfo::DecodeIcon(const Extension* extension,
                           int icon_size,
                           scoped_ptr<SkBitmap>* result) {
  DecodeIcon(extension, icon_size, ExtensionIconSet::MATCH_EXACTLY, result);
}

// static
void IconsInfo::DecodeIconFromPath(const base::FilePath& icon_path,
                                   int icon_size,
                                   scoped_ptr<SkBitmap>* result) {
  if (icon_path.empty())
    return;

  std::string file_contents;
  if (!file_util::ReadFileToString(icon_path, &file_contents)) {
    DLOG(ERROR) << "Could not read icon file: " << icon_path.LossyDisplayName();
    return;
  }

  // Decode the image using WebKit's image decoder.
  const unsigned char* data =
    reinterpret_cast<const unsigned char*>(file_contents.data());
  webkit_glue::ImageDecoder decoder;
  scoped_ptr<SkBitmap> decoded(new SkBitmap());
  *decoded = decoder.Decode(data, file_contents.length());
  if (decoded->empty()) {
    DLOG(ERROR) << "Could not decode icon file: "
                << icon_path.LossyDisplayName();
    return;
  }

  if (decoded->width() != icon_size || decoded->height() != icon_size) {
    DLOG(ERROR) << "Icon file has unexpected size: "
                << base::IntToString(decoded->width()) << "x"
                << base::IntToString(decoded->height());
    return;
  }

  result->swap(decoded);
}

// static
const gfx::ImageSkia& IconsInfo::GetDefaultAppIcon() {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_APP_DEFAULT_ICON);
}

// static
const gfx::ImageSkia& IconsInfo::GetDefaultExtensionIcon() {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_EXTENSION_DEFAULT_ICON);
}

// static
ExtensionResource IconsInfo::GetIconResource(
    const Extension* extension,
    int size,
    ExtensionIconSet::MatchType match_type) {
  std::string path = GetIcons(extension).Get(size, match_type);
  return path.empty() ? ExtensionResource() : extension->GetResource(path);
}

// static
GURL IconsInfo::GetIconURL(const Extension* extension,
                           int size,
                           ExtensionIconSet::MatchType match_type) {
  std::string path = GetIcons(extension).Get(size, match_type);
  return path.empty() ? GURL() : extension->GetResourceURL(path);
}

IconsHandler::IconsHandler() {
}

IconsHandler::~IconsHandler() {
}

bool IconsHandler::Parse(Extension* extension, string16* error) {
  scoped_ptr<IconsInfo> icons_info(new IconsInfo);
  const DictionaryValue* icons_dict = NULL;
  if (!extension->manifest()->GetDictionary(keys::kIcons, &icons_dict)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidIcons);
    return false;
  }

  if (!manifest_handler_helpers::LoadIconsFromDictionary(
          icons_dict,
          extension_misc::kExtensionIconSizes,
          extension_misc::kNumExtensionIconSizes,
          &icons_info->icons,
          error)) {
    return false;
  }

  extension->SetManifestData(keys::kIcons, icons_info.release());
  return true;
}

}  // namespace extensions
