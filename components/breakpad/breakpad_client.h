// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREAKPAD_BREAKPAD_CLIENT_H_
#define COMPONENTS_BREAKPAD_BREAKPAD_CLIENT_H_

#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {
class FilePath;
}

namespace breakpad {

class BreakpadClient;

// Setter and getter for the client.  The client should be set early, before any
// breakpad code is called, and should stay alive throughout the entire runtime.
void SetBreakpadClient(BreakpadClient* client);

// Breakpad's embedder API should only be used by breakpad.
BreakpadClient* GetBreakpadClient();

// Interface that the embedder implements.
class BreakpadClient {
 public:
  BreakpadClient();
  virtual ~BreakpadClient();

#if defined(OS_WIN)
  // Returns true if an alternative location to store the minidump files was
  // specified. Returns true if |crash_dir| was set.
  virtual bool GetAlternativeCrashDumpLocation(base::FilePath* crash_dir);

  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(const base::FilePath& exe_path,
                                        base::string16* product_name,
                                        base::string16* version,
                                        base::string16* special_build);
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir);

#if defined(OS_POSIX)
  // Sets a function that'll be invoked to dump the current process when
  // without crashing.
  virtual void SetDumpWithoutCrashingFunction(void (*function)());
#endif
};

}  // namespace breakpad

#endif  // COMPONENTS_BREAKPAD_BREAKPAD_CLIENT_H_
