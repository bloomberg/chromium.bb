// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_PATHS_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_PATHS_H_

#include "base/files/file_path.h"

namespace component_updater {

enum {
  PATH_START = 10000,
  DIR_COMPONENT_CLD2 = PATH_START,  // Directory that contains component-updated
                                    // Compact Language Detector files.
  DIR_RECOVERY_BASE,                // Full path to the dir for Recovery
                                    // component.
  DIR_SWIFT_SHADER,                 // Path to the SwiftShader component.
  DIR_SW_REPORTER,                  // Path to the SwReporter component.
  PATH_END
};

// Call once to register the provider for the path keys defined above.
// |components_root_key| is the path provider key defining where the
// components should be installed.
void RegisterPathProvider(const base::FilePath& components_root_key);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_PATHS_H_
