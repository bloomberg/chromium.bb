// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_PACKAGED_SERVICES_MANIFEST_H_
#define CONTENT_PUBLIC_APP_CONTENT_PACKAGED_SERVICES_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace content {

// Returns the service manifest for the "content_packaged_services" service.
// This is a singleton service embedded in the browser process, and it exists
// to package other in- and out-of-process services hosted by Content or its
// embedder.
//
// TODO(https://crbug.com/689159): This can be removed once process launching is
// moved out of Content and into the Service Manager. Instead of this hack, we
// should have a Service Manager embedder API for plugging in manifests at
// startup, as well as for hooking up in- and out-of-process service
// instantiation logic for individual services.
const service_manager::Manifest& GetContentPackagedServicesManifest();

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_PACKAGED_SERVICES_MANIFEST_H_
