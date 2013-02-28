// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_

namespace gfx {
class ImageSkia;
}

namespace extensions {

class InstallObserver {
 public:
  virtual void OnBeginExtensionInstall(
      const std::string& extension_id,
      const std::string& extension_name,
      const gfx::ImageSkia& installing_icon,
      bool is_app) = 0;

  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) = 0;

  virtual void OnInstallFailure(const std::string& extension_id) = 0;

 protected:
  virtual ~InstallObserver() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_
