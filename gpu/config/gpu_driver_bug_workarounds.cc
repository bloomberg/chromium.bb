// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_driver_bug_workarounds.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "gpu/config/gpu_switches.h"

namespace {
// Construct GpuDriverBugWorkarounds from a set of enabled workaround IDs.
void IntSetToWorkarounds(const std::vector<int>& enabled_workarounds,
                         gpu::GpuDriverBugWorkarounds* workarounds) {
  DCHECK(workarounds);
  for (auto ID : enabled_workarounds) {
    switch (ID) {
#define GPU_OP(type, name)    \
  case gpu::type:             \
    workarounds->name = true; \
    break;
      GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
      default:
        NOTIMPLEMENTED();
    }
  }
  if (workarounds->max_texture_size_limit_4096)
    workarounds->max_texture_size = 4096;

  if (workarounds->max_fragment_uniform_vectors_32)
    workarounds->max_fragment_uniform_vectors = 32;
  if (workarounds->max_varying_vectors_16)
    workarounds->max_varying_vectors = 16;
  if (workarounds->max_vertex_uniform_vectors_256)
    workarounds->max_vertex_uniform_vectors = 256;

  if (workarounds->max_copy_texture_chromium_size_1048576)
    workarounds->max_copy_texture_chromium_size = 1048576;
  if (workarounds->max_copy_texture_chromium_size_262144)
    workarounds->max_copy_texture_chromium_size = 262144;
}

// Process a string of wordaround type IDs (seperated by ',') and set up
// the corresponding Workaround flags.
void StringToWorkarounds(const std::string& types,
                         gpu::GpuDriverBugWorkarounds* workarounds) {
  std::vector<int> IDs;
  for (const auto& piece : base::SplitStringPiece(
           types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int number = 0;
    bool succeed = base::StringToInt(piece, &number);
    DCHECK(succeed);
    IDs.push_back(number);
  }
  IntSetToWorkarounds(IDs, workarounds);
}

}  // anonymous namespace

namespace gpu {

GpuDriverBugWorkarounds::GpuDriverBugWorkarounds() {}

GpuDriverBugWorkarounds::GpuDriverBugWorkarounds(
    const std::vector<int>& enabled_driver_bug_workarounds) {
  IntSetToWorkarounds(enabled_driver_bug_workarounds, this);
}

GpuDriverBugWorkarounds::GpuDriverBugWorkarounds(
    const base::CommandLine* command_line) {
  if (!command_line)
    return;

  std::string types =
      command_line->GetSwitchValueASCII(switches::kGpuDriverBugWorkarounds);
  StringToWorkarounds(types, this);
}

GpuDriverBugWorkarounds::GpuDriverBugWorkarounds(
    const GpuDriverBugWorkarounds& other) = default;

GpuDriverBugWorkarounds::~GpuDriverBugWorkarounds() {}

}  // namespace gpu
