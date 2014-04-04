// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/icons_handler.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/manifest_handler_helpers.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"

namespace extensions {

namespace keys = manifest_keys;

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

bool IconsHandler::Parse(Extension* extension, base::string16* error) {
  scoped_ptr<IconsInfo> icons_info(new IconsInfo);
  const base::DictionaryValue* icons_dict = NULL;
  if (!extension->manifest()->GetDictionary(keys::kIcons, &icons_dict)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidIcons);
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

bool IconsHandler::Validate(const Extension* extension,
                            std::string* error,
                            std::vector<InstallWarning>* warnings) const {
  return extension_file_util::ValidateExtensionIconSet(
      IconsInfo::GetIcons(extension),
      extension,
      IDS_EXTENSION_LOAD_ICON_FAILED,
      error);
}

const std::vector<std::string> IconsHandler::Keys() const {
  return SingleKey(keys::kIcons);
}

}  // namespace extensions
