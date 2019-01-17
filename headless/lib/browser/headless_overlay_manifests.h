// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_OVERLAY_MANIFESTS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_OVERLAY_MANIFESTS_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace headless {

// Returns the manifest headless Chrome amends to Content's content_browser
// service manifest. This allows headless Chrome to extend the capabilities
// exposed and/or required by content_browser service instances.
const service_manager::Manifest& GetHeadlessContentBrowserOverlayManifest();

// Returns the manifest headless Chrome amends to Content's
// content_packaged_services service manifest. This allows headless Chrome to
// extend the set of in- and out-of- process services packaged by the browser.
const service_manager::Manifest&
GetHeadlessContentPackagedServicesOverlayManifest();

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_OVERLAY_MANIFESTS_H_
