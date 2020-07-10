// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_COORDINATOR_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_COORDINATOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"

struct AddToHomescreenParams;

namespace banners {
class AppBannerManager;
}

// AddToHomescreenCoordinator is the C++ counterpart of org.chromium.chrome.
// browser.webapps.addtohomescreen.AddToHomescreenCoordinator in Java.
class AddToHomescreenCoordinator {
 public:
  // Called for showing the add-to-homescreen UI for AppBannerManager.
  static bool ShowForAppBanner(
      base::WeakPtr<banners::AppBannerManager> weak_manager,
      std::unique_ptr<AddToHomescreenParams> params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          event_callback);

  AddToHomescreenCoordinator() = delete;
  AddToHomescreenCoordinator(const AddToHomescreenCoordinator&) = delete;
  AddToHomescreenCoordinator& operator=(const AddToHomescreenCoordinator&) =
      delete;
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_COORDINATOR_H_
