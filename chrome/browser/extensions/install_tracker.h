// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_H_

#include "base/observer_list.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

namespace gfx {
class ImageSkia;
}

namespace extensions {

class InstallTracker : public ProfileKeyedService {
 public:
  InstallTracker();
  virtual ~InstallTracker();

  void AddObserver(InstallObserver* observer);
  void RemoveObserver(InstallObserver* observer);

  void OnBeginExtensionInstall(
      const std::string& extension_id,
      const std::string& extension_name,
      const gfx::ImageSkia& installing_icon,
      bool is_app);
  void OnDownloadProgress(const std::string& extension_id,
                          int percent_downloaded);
  void OnInstallFailure(const std::string& extension_id);

  // Overriddes for ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

 private:
  ObserverList<InstallObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(InstallTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_H_
