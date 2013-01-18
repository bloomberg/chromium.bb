// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_COMMAND_H_
#define CHROME_COMMON_EXTENSIONS_COMMAND_H_

#include <string>
#include <map>

#include "base/string16.h"
#include "ui/base/accelerators/accelerator.h"

namespace base {
class DictionaryValue;
}

namespace extensions {
class Extension;
}

namespace extensions {

class Command {
 public:
  Command();
  Command(const std::string& command_name,
          const string16& description,
          const std::string& accelerator);
  ~Command();

  // The platform value for the Command.
  static std::string CommandPlatform();

  // Parse a string as an accelerator. If the accelerator is unparsable then
  // a generic ui::Accelerator object will be returns (with key_code Unknown).
  static ui::Accelerator StringToAccelerator(const std::string& accelerator);

  // Parse the command.
  bool Parse(const base::DictionaryValue* command,
             const std::string& command_name,
             int index,
             string16* error);

  // Convert a Command object from |extension| to a DictionaryValue.
  // |active| specifies whether the command is active or not.
  base::DictionaryValue* ToValue(
      const Extension* extension, bool active) const;

  // Accessors:
  const std::string& command_name() const { return command_name_; }
  const ui::Accelerator& accelerator() const { return accelerator_; }
  const string16& description() const { return description_; }

  // Setter:
  void set_accelerator(ui::Accelerator accelerator) {
    accelerator_ = accelerator;
  }

 private:
  std::string command_name_;
  ui::Accelerator accelerator_;
  string16 description_;
};

// A mapping of command name (std::string) to a command object.
typedef std::map<std::string, Command> CommandMap;

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_COMMAND_H_
