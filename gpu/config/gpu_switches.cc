// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switches.h"

namespace switches {

// Passes if it's AMD switchable dual GPUs from browser process to GPU process.
const char kAMDSwitchable[] = "amd-switchable";

// Disable workarounds for various GPU driver bugs.
const char kDisableGpuDriverBugWorkarounds[] =
    "disable-gpu-driver-bug-workarounds";

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGpuRasterization and kForceGpuRasterization flags.
const char kDisableGpuRasterization[] = "disable-gpu-rasterization";

// Allow heuristics to determine when a layer tile should be drawn with the
// Skia GPU backend. Only valid with GPU accelerated compositing +
// impl-side painting.
const char kEnableGpuRasterization[] = "enable-gpu-rasterization";

// Turns on out of process raster for the renderer whenever gpu raster
// would have been used.  Enables the chromium_raster_transport extension.
const char kEnableOOPRasterization[] = "enable-oop-rasterization";

// Passes encoded GpuPreferences to GPU process.
const char kGpuPreferences[] = "gpu-preferences";

// Passes active gpu vendor id from browser process to GPU process.
const char kGpuActiveVendorID[] = "gpu-active-vendor-id";

// Passes active gpu device id from browser process to GPU process.
const char kGpuActiveDeviceID[] = "gpu-active-device-id";

// Passes gpu device_id from browser process to GPU process.
const char kGpuDeviceID[] = "gpu-device-id";

// Passes gpu driver_vendor from browser process to GPU process.
const char kGpuDriverVendor[] = "gpu-driver-vendor";

// Passes gpu driver_version from browser process to GPU process.
const char kGpuDriverVersion[] = "gpu-driver-version";

// Passes gpu driver_date from browser process to GPU process.
const char kGpuDriverDate[] = "gpu-driver-date";

// Passes secondary gpu vendor ids from browser process to GPU process.
const char kGpuSecondaryVendorIDs[] = "gpu-secondary-vendor-ids";

// Passes secondary gpu device ids from browser process to GPU process.
const char kGpuSecondaryDeviceIDs[] = "gpu-secondary-device-ids";

// Testing switch to not launch the gpu process for full gpu info collection.
const char kGpuTestingNoCompleteInfoCollection[] =
    "gpu-no-complete-info-collection";

// Passes gpu vendor_id from browser process to GPU process.
const char kGpuVendorID[] = "gpu-vendor-id";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[] = "ignore-gpu-blacklist";

// Select a different set of GPU blacklist entries with the specificed
// test_group ID.
const char kGpuBlacklistTestGroup[] = "gpu-blacklist-test-group";

// Select a different set of GPU driver bug list entries with the specificed
// test_group ID.
const char kGpuDriverBugListTestGroup[] = "gpu-driver-bug-list-test-group";

}  // namespace switches
