// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_ICON_LOADER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_APP_ICON_LOADER_IMPL_H_

#include <map>
#include <string>

#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/ui/ash/app_icon_loader.h"

class Profile;

namespace extensions {
class Extension;
}

namespace ash {

// Default implementation of ash::AppIconLoader that interacts with the
// ExtensionService and ImageLoadingTracker to load images.
class AppIconLoaderImpl : public AppIconLoader,
                          public extensions::IconImage::Observer {
 public:
  AppIconLoaderImpl(Profile* profile, int icon_size,
                    AppIconLoader::Delegate* delegate);
  virtual ~AppIconLoaderImpl();

  // AppIconLoader overrides:
  virtual void FetchImage(const std::string& id) OVERRIDE;
  virtual void ClearImage(const std::string& id) OVERRIDE;
  virtual void UpdateImage(const std::string& id) OVERRIDE;

  // extensions::IconImage::Observer overrides:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

 private:
  typedef std::map<extensions::IconImage*, std::string> ImageToExtensionIDMap;

  // Builds image for given |id| and |icon|.
  void BuildImage(const std::string& id, const gfx::ImageSkia& icon);

  Profile* profile_;

  // The delegate object which receives the icon images. No ownership.
  AppIconLoader::Delegate* delegate_;

  // Maps from IconImage pointer to the extension id.
  ImageToExtensionIDMap map_;

  const int icon_size_;

  DISALLOW_COPY_AND_ASSIGN(AppIconLoaderImpl);
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_APP_ICON_LOADER_IMPL_H_
