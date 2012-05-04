// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_COMMANDS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_COMMANDS_H_
#pragma once

#include <string>
#include <map>

#include "base/string16.h"
#include "ui/base/accelerators/accelerator.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

class Command {
 public:
  // Define out of line constructor/destructor to please Clang.
  Command();
  ~Command();

  // The platform value for the Command.
  static std::string CommandPlatform();

  // Parse the command.
  bool Parse(base::DictionaryValue* command,
             const std::string& command_name,
             int index,
             string16* error);

  // Accessors:
  const std::string& command_name() const { return command_name_; }
  const ui::Accelerator& accelerator() const { return accelerator_; }
  const std::string& description() const { return description_; }

 private:
  ui::Accelerator ParseImpl(const std::string& shortcut,
                            const std::string& platform_key,
                            int index,
                            string16* error);
  std::string command_name_;
  ui::Accelerator accelerator_;
  std::string description_;
};

// A mapping of command name (std::string) to a command object.
typedef std::map<std::string, Command> CommandMap;

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_COMMANDS_H_
