// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/service_names.h"

namespace content {

// The default service name the browser identifies as when connecting to
// the Service Manager. This must match the name in
// src/content/public/app/mojo/content_browser_manifest.json.
const char kBrowserServiceName[] = "content_browser";

// The default service name used to identify the gpu process when connecting it
// to the Service Manager. This must match the name in
// src/content/public/app/mojo/content_gpu_manifest.json.
const char kGpuServiceName[] = "content_gpu";

// The default service name used to identify plugin processes when connecting
// them to the Service Manager. This must match the name in
// src/content/public/app/mojo/content_plugin_manifest.json.
const char kPluginServiceName[] = "content_plugin";

// The default service name used to identify render processes when connecting
// them to the Service Manager. This must match the name in
// src/content/public/app/mojo/content_renderer_manifest.json.
const char kRendererServiceName[] = "content_renderer";

// The default service name used to identify utility processes when connecting
// them to the Service Manager. This must match the name in
// src/content/public/app/mojo/content_utility_manifest.json.
const char kUtilityServiceName[] = "content_utility";

}  // namespace content
