// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_PACKAGED_SERVICES_MANIFEST_H_
#define IOS_WEB_WEB_PACKAGED_SERVICES_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace web {

// Returns the service manifest for the web_packaged_services service. This
// contains nested manifests for all other services available to the browser,
// with the exception of per-profile services which are packaged by web_browser.
const service_manager::Manifest& GetWebPackagedServicesManifest();

}  // namespace web

#endif  // IOS_WEB_WEB_PACKAGED_SERVICES_MANIFEST_H_
