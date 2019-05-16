// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"

namespace plugin_vm {

const char kPluginVmImageDownloadedSize[] = "PluginVm.Image.DownloadedSize";

void RecordPluginVmImageDownloadedSize(uint64_t bytes_downloaded) {
  uint64_t megabytes_downloaded = bytes_downloaded / (1024 * 1024);
  base::UmaHistogramMemoryLargeMB(kPluginVmImageDownloadedSize,
                                  megabytes_downloaded);
}

}  // namespace plugin_vm
