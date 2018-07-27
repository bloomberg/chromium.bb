// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ARC_APPS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ARC_APPS_PRIVATE_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

class ArcAppsPrivateGetLaunchableAppsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.getLaunchableApps",
                             ARCAPPSPRIVATE_GETLAUNCHABLEAPPS)

  ArcAppsPrivateGetLaunchableAppsFunction();

 protected:
  ~ArcAppsPrivateGetLaunchableAppsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateGetLaunchableAppsFunction);
};

class ArcAppsPrivateLaunchAppFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.launchApp",
                             ARCAPPSPRIVATE_LAUNCHAPP)

  ArcAppsPrivateLaunchAppFunction();

 protected:
  ~ArcAppsPrivateLaunchAppFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateLaunchAppFunction);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ARC_APPS_PRIVATE_API_H_