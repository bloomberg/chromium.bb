// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_COMMANDS_H_
#define CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_COMMANDS_H_

#include <iosfwd>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/installer/zucchini/zucchini.h"

// Zucchini commands and tools that can be invoked from command-line.

namespace base {

class CommandLine;

}  // namespace base

// Aggregated parameter for Main*() functions, to simplify interface.
struct MainParams {
  const base::CommandLine& command_line;
  const std::vector<base::FilePath>& file_paths;
  std::ostream& out;
  std::ostream& err;
};

// Signature of a Zucchini Command Function.
using CommandFunction = zucchini::status::Code (*)(MainParams);

// Command Function: Patch generation.
zucchini::status::Code MainGen(MainParams params);

// Command Function: Patch application.
zucchini::status::Code MainApply(MainParams params);

// Command Function: Compute CRC-32 of a file.
zucchini::status::Code MainCrc32(MainParams params);

#endif  // CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_COMMANDS_H_
