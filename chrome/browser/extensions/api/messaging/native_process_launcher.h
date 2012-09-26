// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_

#include "base/process.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"

class FilePath;

namespace extensions {

class NativeProcessLauncher {
 public:
  NativeProcessLauncher() {}
  virtual ~NativeProcessLauncher() {}
  virtual bool LaunchNativeProcess(
      const FilePath& path,
      base::ProcessHandle* native_process_handle,
      NativeMessageProcessHost::FileHandle* read_file,
      NativeMessageProcessHost::FileHandle* write_file) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeProcessLauncher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_
