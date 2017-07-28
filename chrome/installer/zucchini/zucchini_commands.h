// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_COMMANDS_H_
#define CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_COMMANDS_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/installer/zucchini/zucchini.h"

// Zucchini commands and tools that can be invoked from command-line.

namespace base {

class CommandLine;

}  // namespace base

// Signature of a Zucchini Command Function.
using CommandFunction =
    zucchini::status::Code (*)(const base::CommandLine&,
                               const std::vector<base::FilePath>&);

// Command Function: Patch generation.
zucchini::status::Code MainGen(const base::CommandLine& command_line,
                               const std::vector<base::FilePath>& fnames);

// Command Function: Patch application.
zucchini::status::Code MainApply(const base::CommandLine& command_line,
                                 const std::vector<base::FilePath>& fnames);

#endif  // CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_COMMANDS_H_
