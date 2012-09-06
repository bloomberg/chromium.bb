// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_GPU_UTIL_H_
#define CHROME_BROWSER_CHROME_GPU_UTIL_H_

class CommandLine;

namespace gpu_util {

// Sets up force-compositing-mode and threaded compositing field trials.
void InitializeCompositingFieldTrial();

// Load GPU Blacklist, collect preliminary gpu info, and compute preliminary
// gpu feature flags.
void InitializeGpuDataManager(const CommandLine& command_line);

}  // namespace gpu_util

#endif  // CHROME_BROWSER_CHROME_GPU_UTIL_H_

