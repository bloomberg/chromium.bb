// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/switches.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

namespace switches {
namespace {

constexpr uint32_t kDefaultNumberOfFramesToDeadline = 4;

}  // namespace

// The default number of the BeginFrames to wait to activate a surface with
// dependencies.
const char kDeadlineToSynchronizeSurfaces[] =
    "deadline-to-synchronize-surfaces";

// Disable surface lifetime management using surface references. This enables
// adding surface sequences and disables adding temporary references. This flag
// is only checked on Android, other platforms always have surface references
// enabled.
const char kDisableSurfaceReferences[] = "disable-surface-references";

// Enables multi-client Surface synchronization. In practice, this indicates
// that LayerTreeHost expects to be given a valid viz::LocalSurfaceId provided
// by the parent compositor.
const char kEnableSurfaceSynchronization[] = "enable-surface-synchronization";

uint32_t GetDeadlineToSynchronizeSurfaces() {
  std::string deadline_to_synchronize_surfaces_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDeadlineToSynchronizeSurfaces);
  uint32_t number_of_frames_to_activation_deadline;
  if (!base::StringToUint(deadline_to_synchronize_surfaces_string,
                          &number_of_frames_to_activation_deadline)) {
    return kDefaultNumberOfFramesToDeadline;
  }
  return number_of_frames_to_activation_deadline;
}

}  // namespace switches
