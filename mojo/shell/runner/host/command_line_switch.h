// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_RUNNER_HOST_COMMAND_LINE_SWITCH_H_
#define MOJO_SHELL_RUNNER_HOST_COMMAND_LINE_SWITCH_H_

#include <string>

namespace mojo {
namespace shell {

struct CommandLineSwitch {
  CommandLineSwitch() : is_switch(true) {}

  // If false only the key is used and the switch is treated as a single value.
  bool is_switch;
  std::string key;
  std::string value;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_RUNNER_HOST_COMMAND_LINE_SWITCH_H_
