// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_EXTENSION_BADGED_ICON_IMAGE_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_EXTENSION_BADGED_ICON_IMAGE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/launcher_search/extension_badged_icon_image.h"
#include "extensions/browser/extension_icon_image.h"

namespace app_list {

// ExtensionBadgedIconImageImpl loads extension icon and icon resource from
// extension.
class ExtensionBadgedIconImageImpl : public ExtensionBadgedIconImage,
                                     extensions::IconImage::Observer {
 public:
  ExtensionBadgedIconImageImpl(
      const GURL& custom_icon_url,
      Profile* profile,
      const extensions::Extension* extension,
      const int icon_dimension,
      scoped_ptr<chromeos::launcher_search_provider::ErrorReporter>
          error_reporter);
  ~ExtensionBadgedIconImageImpl() override;
  const gfx::ImageSkia& LoadExtensionIcon() override;
  void LoadIconResourceFromExtension() override;

  // extensions::IconImage::Observer override.
  void OnExtensionIconImageChanged(extensions::IconImage* image) override;

  // ImageLoaderImageCallback.
  void OnCustomIconImageLoaded(const gfx::Image& image);

 private:
  scoped_ptr<extensions::IconImage> extension_icon_image_;

  base::WeakPtrFactory<ExtensionBadgedIconImageImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBadgedIconImageImpl);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_EXTENSION_BADGED_ICON_IMAGE_IMPL_H_
