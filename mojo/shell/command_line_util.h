// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_COMMAND_LINE_UTIL_H_
#define SHELL_COMMAND_LINE_UTIL_H_

#include "mojo/shell/context.h"

namespace mojo {
namespace shell {

// Parse the given arg, looking for an --args-for switch. If this is not the
// case, returns |false|. Otherwise, returns |true| and set |*value| to the
// value of the switch.
bool ParseArgsFor(const std::string& arg, std::string* value);

// The value of app_url_and_args is "<mojo_app_url> [<args>...]", where args
// is a list of "configuration" arguments separated by spaces. If one or more
// arguments are specified they will be available when the Mojo application
// is initialized. This returns the mojo_app_url, and set args to the list of
// arguments.
GURL GetAppURLAndArgs(Context* context,
                      const std::string& app_url_and_args,
                      std::vector<std::string>* args);

// Apply arguments for an application from a line with the following format:
// '--args-for=application_url arg1 arg2 arg3'
// This does nothing if the line has not the right format.
void ApplyApplicationArgs(Context* context, const std::string& args);

// Run all application defined on the command line, using the given context.
void RunCommandLineApps(Context* context);

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_COMMAND_LINE_UTIL_H_
