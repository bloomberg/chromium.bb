// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"

namespace nacl {

void CopyNaClCommandLineArguments(CommandLine* cmd_line) {
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (logging::DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  // Propagate the following switches to the NaCl loader command line (along
  // with any associated values) if present in the browser command line.
  // TODO(gregoryd): check which flags of those below can be supported.
  static const char* const kSwitchNames[] = {
    switches::kNoSandbox,
    switches::kTestNaClSandbox,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kMemoryProfiling,
    switches::kEnableNaClDebug,
    switches::kNaClDebugPorts,
    switches::kNaClDebugIP,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));
}

}
