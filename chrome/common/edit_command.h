// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EDIT_COMMAND_H_
#define CHROME_COMMON_EDIT_COMMAND_H_
#pragma once

#include <string>
#include <vector>

// Types related to sending edit commands to the renderer.
struct EditCommand {
  EditCommand() { }
  EditCommand(const std::string& n, const std::string& v)
      : name(n), value(v) {
  }

  std::string name;
  std::string value;
};

typedef std::vector<EditCommand> EditCommands;

#endif  // CHROME_COMMON_EDIT_COMMAND_H_
