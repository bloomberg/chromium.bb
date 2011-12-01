// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/debug_flags.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_switches.h"

bool DebugFlags::ProcessDebugFlags(CommandLine* command_line,
                                   content::ProcessType type,
                                   bool is_in_sandbox) {
  bool should_help_child = false;
  const CommandLine& current_cmd_line = *CommandLine::ForCurrentProcess();
  if (current_cmd_line.HasSwitch(switches::kDebugChildren)) {
    // Look to pass-on the kDebugOnStart flag.
    std::string value = current_cmd_line.GetSwitchValueASCII(
        switches::kDebugChildren);
    if (value.empty() ||
        (type == content::PROCESS_TYPE_WORKER &&
         value == switches::kWorkerProcess) ||
        (type == content::PROCESS_TYPE_RENDERER &&
         value == switches::kRendererProcess) ||
        (type == content::PROCESS_TYPE_PLUGIN &&
         value == switches::kPluginProcess)) {
      command_line->AppendSwitch(switches::kDebugOnStart);
      should_help_child = true;
    }
    command_line->AppendSwitchASCII(switches::kDebugChildren, value);
  } else if (current_cmd_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value = current_cmd_line.GetSwitchValueASCII(
        switches::kWaitForDebuggerChildren);
    if (value.empty() ||
        (type == content::PROCESS_TYPE_WORKER &&
         value == switches::kWorkerProcess) ||
        (type == content::PROCESS_TYPE_RENDERER &&
         value == switches::kRendererProcess) ||
        (type == content::PROCESS_TYPE_PLUGIN &&
         value == switches::kPluginProcess)) {
      command_line->AppendSwitch(switches::kWaitForDebugger);
    }
    command_line->AppendSwitchASCII(switches::kWaitForDebuggerChildren, value);
  }
  return should_help_child;
}
