// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_ICON_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "extensions/browser/extension_icon_image.h"

class Profile;

namespace extensions {

// Implementation of AppIconLoader that interacts with the ExtensionService and
// ImageLoader to load images.
class ExtensionAppIconLoader : public AppIconLoader,
                               public extensions::IconImage::Observer {
 public:
  ExtensionAppIconLoader(Profile* profile, int icon_size,
                         AppIconLoaderDelegate* delegate);
  ~ExtensionAppIconLoader() override;

  // AppIconLoader overrides:
  bool CanLoadImageForApp(const std::string& id) override;
  void FetchImage(const std::string& id) override;
  void ClearImage(const std::string& id) override;
  void UpdateImage(const std::string& id) override;

  // extensions::IconImage::Observer overrides:
  void OnExtensionIconImageChanged(extensions::IconImage* image) override;

 private:
  using ExtensionIDToImageMap =
      std::map<std::string, std::unique_ptr<extensions::IconImage>>;

  // Builds image for given |id| and |icon|.
  void BuildImage(const std::string& id, const gfx::ImageSkia& icon);

  // Maps from extension id to IconImage pointer.
  ExtensionIDToImageMap map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppIconLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_ICON_LOADER_H_
