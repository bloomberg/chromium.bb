// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_background_services_context.h"

namespace content {

DevToolsBackgroundServicesContext::DevToolsBackgroundServicesContext(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : browser_context_(browser_context),
      service_worker_context_(std::move(service_worker_context)) {}

DevToolsBackgroundServicesContext::~DevToolsBackgroundServicesContext() =
    default;

}  // namespace content
