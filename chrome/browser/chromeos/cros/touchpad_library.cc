// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/touchpad_library.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "content/browser/browser_thread.h"

namespace chromeos {
namespace {
const char* kTpControl = "/opt/google/touchpad/tpcontrol";
}  // namespace

class TouchpadLibraryImpl : public TouchpadLibrary {
 public:
  TouchpadLibraryImpl() {}
  virtual ~TouchpadLibraryImpl() {}

  void SetSensitivity(int value) {
    // Run this on the FILE thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this,
                            &TouchpadLibraryImpl::SetSensitivity, value));
      return;
    }
    std::vector<std::string> argv;
    argv.push_back(kTpControl);
    argv.push_back("sensitivity");
    argv.push_back(StringPrintf("%d", value));

    base::LaunchOptions options;
    options.wait = false;  // Launch asynchronously.

    base::LaunchProcess(CommandLine(argv), options);
  }

  void SetTapToClick(bool enabled) {
    // Run this on the FILE thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this,
                            &TouchpadLibraryImpl::SetTapToClick, enabled));
      return;
    }
    std::vector<std::string> argv;
    argv.push_back(kTpControl);
    argv.push_back("taptoclick");
    argv.push_back(enabled ? "on" : "off");

    base::LaunchOptions options;
    options.wait = false;  // Launch asynchronously.

    base::LaunchProcess(CommandLine(argv), options);
  }

  DISALLOW_COPY_AND_ASSIGN(TouchpadLibraryImpl);
};

class TouchpadLibraryStubImpl : public TouchpadLibrary {
 public:
  TouchpadLibraryStubImpl() {}
  virtual ~TouchpadLibraryStubImpl() {}
  void SetSensitivity(int value) {}
  void SetTapToClick(bool enabled) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchpadLibraryStubImpl);
};

// static
TouchpadLibrary* TouchpadLibrary::GetImpl(bool stub) {
  if (stub)
    return new TouchpadLibraryStubImpl();
  else
    return new TouchpadLibraryImpl();
}

}  // namespace chromeos

// Needed for NewRunnableMethod() call above.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::TouchpadLibraryImpl);
