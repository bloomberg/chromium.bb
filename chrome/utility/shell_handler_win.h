// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SHELL_HANDLER_WIN_H_
#define CHROME_UTILITY_SHELL_HANDLER_WIN_H_

#include <Windows.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/tuple.h"
#include "chrome/utility/utility_message_handler.h"

namespace base {
class FilePath;
}  // namespace base

typedef std::vector<Tuple2<base::string16, base::string16> >
    GetOpenFileNameFilter;

struct ChromeUtilityMsg_GetSaveFileName_Params;

// Handles requests to execute shell operations. Used to protect the browser
// process from instability due to 3rd-party shell extensions. Must be invoked
// in a non-sandboxed utility process.
class ShellHandler : public UtilityMessageHandler {
 public:
  ShellHandler();
  virtual ~ShellHandler();

  // IPC::Listener implementation
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnOpenItemViaShell(const base::FilePath& full_path);

  void OnGetOpenFileName(
    HWND owner,
    DWORD flags,
    const GetOpenFileNameFilter& filter,
    const base::FilePath& initial_directory,
    const base::FilePath& filename);

  void OnGetSaveFileName(const ChromeUtilityMsg_GetSaveFileName_Params& params);

  DISALLOW_COPY_AND_ASSIGN(ShellHandler);
};

#endif  // CHROME_UTILITY_SHELL_HANDLER_WIN_H_
