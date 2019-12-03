// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_guest_ui.h"

#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources_map.h"
#include "chromeos/grit/chromeos_help_app_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

// static
content::WebUIDataSource* CreateHelpAppGuestDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIHelpAppGuestHost);
  source->AddResourcePath("app.html", IDR_HELP_APP_APP_HTML);
  source->AddResourcePath("bootstrap.js", IDR_HELP_APP_BOOTSTRAP_JS);

  // Add all resources from chromeos_media_app_bundle.pak.
  for (size_t i = 0; i < kChromeosHelpAppBundleResourcesSize; i++) {
    source->AddResourcePath(kChromeosHelpAppBundleResources[i].name,
                            kChromeosHelpAppBundleResources[i].value);
  }

  // TODO(crbug.com/1023700): Better solution before launch.
  source->DisableDenyXFrameOptions();
  return source;
}

}  // namespace chromeos
