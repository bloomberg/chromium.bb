// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_

#include "base/metrics/histogram_functions.h"

namespace plugin_vm {

extern const char kPluginVmImageDownloadedSize[];

void RecordPluginVmImageDownloadedSize(uint64_t bytes_downloaded);

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_
