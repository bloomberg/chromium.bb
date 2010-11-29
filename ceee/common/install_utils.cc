// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities related to installation of the CEEE.

#include "ceee/common/install_utils.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/installer/util/util_constants.h"

namespace installer_util {
  namespace switches {
    // TODO(joi@chromium.org) Move to chrome/installer/util_constants.h
    // when we refactor this logic to be in the installer rather than
    // on the registration entrypoints.
    const wchar_t kEnableCeee[] = L"enable-ceee";
    const wchar_t kEnableFfCeee[] = L"enable-ff-ceee";
  }
}

namespace ceee_install_utils {

bool ShouldRegisterImpl(bool check_firefox) {
  // First check if it's a developer running us explicitly.
  FilePath exe_path;
  if (PathService::Get(base::FILE_EXE, &exe_path)) {
    if (exe_path.BaseName() == FilePath(L"regsvr32.exe")) {
      return true;
    }
  }

  // Failing that, it's some kind of install scenario, so the
  // --enable-ceee flag must be provided. It should be ignored
  // unless --chrome-frame is also specified, so we check for
  // both.
  //
  // If check_firefox is true, the --enable-ff-ceee flag must
  // also be provided.
  CommandLine current_command_line(CommandLine::NO_PROGRAM);
  current_command_line.ParseFromString(::GetCommandLine());
  if (current_command_line.HasSwitch(installer_util::switches::kEnableCeee) &&
      current_command_line.HasSwitch(installer_util::switches::kChromeFrame) &&
      (!check_firefox || current_command_line.HasSwitch(
          installer_util::switches::kEnableFfCeee))) {
    return true;
  } else {
    return false;
  }
}

bool ShouldRegisterCeee() {
  return ShouldRegisterImpl(false);
}

bool ShouldRegisterFfCeee() {
  return ShouldRegisterImpl(true);
}

}  // namespace ceee_install_utils
