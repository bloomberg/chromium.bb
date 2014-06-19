// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_CONVERTER_H_
#define CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_CONVERTER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/drive/drive_app_registry.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace extensions {
class CrxInstaller;
class Extension;
}

// DriveAppConverter creates and installs a local URL app for the given
// DriveAppInfo into the given profile.
class DriveAppConverter : public extensions::InstallObserver {
 public:
  typedef base::Callback<void(const DriveAppConverter*, bool success)>
      FinishedCallback;

  DriveAppConverter(Profile* profile,
                    const drive::DriveAppInfo& drive_app_info,
                    const FinishedCallback& finished_callback);
  virtual ~DriveAppConverter();

  void Start();
  bool IsStarted() const;

  bool IsInstalling(const std::string& app_id) const;

  const drive::DriveAppInfo& drive_app_info() const { return drive_app_info_; }
  const extensions::Extension* extension() const { return extension_; }
  bool is_new_install() const { return is_new_install_; }

 private:
  class IconFetcher;

  // Callbacks from IconFetcher.
  void OnIconFetchComplete(const IconFetcher* fetcher);

  void StartInstall();
  void PostInstallCleanUp();

  // extensions::InstallObserver:
  virtual void OnFinishCrxInstall(const std::string& extension_id,
                                  bool success) OVERRIDE;

  Profile* profile_;
  const drive::DriveAppInfo drive_app_info_;

  WebApplicationInfo web_app_;
  const extensions::Extension* extension_;
  bool is_new_install_;

  ScopedVector<IconFetcher> fetchers_;
  scoped_refptr<extensions::CrxInstaller> crx_installer_;

  FinishedCallback finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppConverter);
};

#endif  // CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_CONVERTER_H_
