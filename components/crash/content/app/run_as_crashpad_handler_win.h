// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_
#define COMPONENTS_CRASH_CONTENT_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_

namespace base {
class CommandLine;
}

namespace crash_reporter {

// Helper for running an embedded copy of crashpad_handler. Searches for and
// removes --(process_type_switch)=xyz arguments in the command line, and all
// options starting with '/' (for "/prefetch:N"), and then runs
// crashpad::HandlerMain with the remaining arguments.
//
// Normally, pass switches::kProcessType for process_type_switch. It's accepted
// as a parameter because this component does not have access to content/, where
// that variable lives.
int RunAsCrashpadHandler(const base::CommandLine& command_line,
                         const char* process_type_switch);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_
