// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_file_util.h"

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"

using extensions::Extension;
using extensions::ExtensionResource;
using extensions::Manifest;

namespace {

// Add the image paths contained in the |icon_set| to |image_paths|.
void AddPathsFromIconSet(const ExtensionIconSet& icon_set,
                         std::set<base::FilePath>* image_paths) {
  // TODO(viettrungluu): These |FilePath::FromUTF8Unsafe()| indicate that we're
  // doing something wrong.
  for (ExtensionIconSet::IconMap::const_iterator iter = icon_set.map().begin();
       iter != icon_set.map().end(); ++iter) {
    image_paths->insert(base::FilePath::FromUTF8Unsafe(iter->second));
  }
}

}  // namespace

namespace extension_file_util {

std::set<base::FilePath> GetBrowserImagePaths(const Extension* extension) {
  std::set<base::FilePath> image_paths;

  AddPathsFromIconSet(extensions::IconsInfo::GetIcons(extension), &image_paths);

  // Theme images
  const base::DictionaryValue* theme_images =
      extensions::ThemeInfo::GetImages(extension);
  if (theme_images) {
    for (base::DictionaryValue::Iterator it(*theme_images); !it.IsAtEnd();
         it.Advance()) {
      base::FilePath::StringType path;
      if (it.value().GetAsString(&path))
        image_paths.insert(base::FilePath(path));
    }
  }

  const extensions::ActionInfo* page_action =
      extensions::ActionInfo::GetPageActionInfo(extension);
  if (page_action && !page_action->default_icon.empty())
    AddPathsFromIconSet(page_action->default_icon, &image_paths);

  const extensions::ActionInfo* browser_action =
      extensions::ActionInfo::GetBrowserActionInfo(extension);
  if (browser_action && !browser_action->default_icon.empty())
    AddPathsFromIconSet(browser_action->default_icon, &image_paths);

  return image_paths;
}

}  // namespace extension_file_util
