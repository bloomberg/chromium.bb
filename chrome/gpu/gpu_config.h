// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_CONFIG_H_
#define CHROME_GPU_GPU_CONFIG_H_

// This file declares common preprocessor configuration for the GPU process.

#include "build/build_config.h"

#if defined(OS_LINUX) && !defined(ARCH_CPU_ARMEL)

// Only define GLX support for Intel CPUs for now until we can get the
// proper dependencies and build setup for ARM.
#define GPU_USE_GLX

#endif

#endif  // CHROME_GPU_GPU_CONFIG_H_
