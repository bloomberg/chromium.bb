// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PROCESS_TYPE_H_
#define CONTENT_PUBLIC_COMMON_PROCESS_TYPE_H_
#pragma once

#include <string>

namespace content {

// Defines the different process types.
// NOTE: Do not remove or reorder the elements in this enum, and only add new
// items at the end, right before MAX_PROCESS. We depend on these specific
// values in histograms.
enum ProcessType {
  PROCESS_TYPE_UNKNOWN = 1,
  PROCESS_TYPE_BROWSER,
  PROCESS_TYPE_RENDERER,
  PROCESS_TYPE_PLUGIN,
  PROCESS_TYPE_WORKER,
  PROCESS_TYPE_NACL_LOADER,
  PROCESS_TYPE_UTILITY,
  PROCESS_TYPE_PROFILE_IMPORT,
  PROCESS_TYPE_ZYGOTE,
  PROCESS_TYPE_SANDBOX_HELPER,
  PROCESS_TYPE_NACL_BROKER,
  PROCESS_TYPE_GPU,
  PROCESS_TYPE_PPAPI_PLUGIN,
  PROCESS_TYPE_PPAPI_BROKER,
  PROCESS_TYPE_MAX
};

// Returns an English name of the process type, should only be used for non
// user-visible strings, or debugging pages like about:memory.
std::string GetProcessTypeNameInEnglish(ProcessType type);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PROCESS_TYPE_H_
