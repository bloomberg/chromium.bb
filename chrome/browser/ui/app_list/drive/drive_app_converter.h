// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_DRIVE_DRIVE_APP_CONVERTER_H_
#define CHROME_BROWSER_UI_APP_LIST_DRIVE_DRIVE_APP_CONVERTER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/drive/drive_app_registry.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/common/web_application_info.h"

namespace extensions {
class CrxInstaller;
class Extension;
}

class Profile;

// DriveAppConverter creates and installs a local URL app for the given
// DriveAppInfo into the given profile.
class DriveAppConverter : public extensions::InstallObserver {
 public:
  typedef base::Callback<void(const DriveAppConverter*, bool success)>
      FinishedCallback;

  DriveAppConverter(Profile* profile,
                    const drive::DriveAppInfo& app_info,
                    const FinishedCallback& finished_callback);
  virtual ~DriveAppConverter();

  void Start();

  const drive::DriveAppInfo& app_info() const { return app_info_; }
  const extensions::Extension* app() const { return app_; }

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
  const drive::DriveAppInfo app_info_;

  WebApplicationInfo web_app_;
  const extensions::Extension* app_;

  ScopedVector<IconFetcher> fetchers_;
  scoped_refptr<extensions::CrxInstaller> crx_installer_;

  FinishedCallback finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppConverter);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_DRIVE_DRIVE_APP_CONVERTER_H_
