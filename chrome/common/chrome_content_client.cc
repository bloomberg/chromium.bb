// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "chrome/common/child_process_logging.h"

namespace chrome {

void ChromeContentClient::SetActiveURL(const GURL& url) {
}

void ChromeContentClient::SetGpuInfo(const GPUInfo& gpu_info) {
  child_process_logging::SetGpuInfo(gpu_info);
}

void ChromeContentClient::PluginProcessStarted() {
}

}  // namespace chrome
