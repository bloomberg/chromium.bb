// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/run_as_crashpad_handler_win.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process/memory.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

namespace crash_reporter {

int RunAsCrashpadHandler(const base::CommandLine& command_line,
                         const char* process_type_switch) {
  // Make sure this process terminates on OOM in the same mode as other Chrome
  // processes.
  base::EnableTerminationOnOutOfMemory();

  // If the handler is started with --monitor-self, it'll need a ptype
  // annotation set. It'll normally set one itself by being invoked with
  // --monitor-self-annotation=ptype=crashpad-handler, but that leaves a window
  // during self-monitoring initialization when the ptype is not set at all, so
  // provide one here.
  const std::string process_type =
      command_line.GetSwitchValueASCII(process_type_switch);
  if (!process_type.empty()) {
    crashpad::SimpleStringDictionary* annotations =
        new crashpad::SimpleStringDictionary();
    annotations->SetKeyValue("ptype", process_type.c_str());
    crashpad::CrashpadInfo* crashpad_info =
        crashpad::CrashpadInfo::GetCrashpadInfo();
    DCHECK(!crashpad_info->simple_annotations());
    crashpad_info->set_simple_annotations(annotations);
  }

  std::vector<base::string16> argv = command_line.argv();
  const base::string16 process_type_arg_prefix =
      base::string16(L"--") + base::UTF8ToUTF16(process_type_switch) + L"=";
  argv.erase(
      std::remove_if(argv.begin(), argv.end(),
                     [&process_type_arg_prefix](const base::string16& str) {
                       return base::StartsWith(str, process_type_arg_prefix,
                                               base::CompareCase::SENSITIVE) ||
                              (!str.empty() && str[0] == L'/');
                     }),
      argv.end());

  std::unique_ptr<char* []> argv_as_utf8(new char*[argv.size() + 1]);
  std::vector<std::string> storage;
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
    storage.push_back(base::UTF16ToUTF8(argv[i]));
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;
  argv.clear();
  return crashpad::HandlerMain(static_cast<int>(storage.size()),
                               argv_as_utf8.get(), nullptr);
}

}  // namespace crash_reporter
