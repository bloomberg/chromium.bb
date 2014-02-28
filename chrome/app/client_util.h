// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions that can report details about the
// host operating environment.

#ifndef CHROME_APP_CLIENT_UTIL_H_
#define CHROME_APP_CLIENT_UTIL_H_

#include <windows.h>

#include <string>
#include "base/strings/string16.h"

namespace sandbox {
  struct SandboxInterfaceInfo;
}

// Gets the path of the current exe with a trailing backslash.
base::string16 GetExecutablePath();

// Returns the version in the current module's version resource or the empty
// string if none found.
base::string16 GetCurrentModuleVersion();

// Implements the common aspects of loading the main dll for both chrome and
// chromium scenarios, which are in charge of implementing two abstract
// methods: GetRegistryPath() and OnBeforeLaunch().
class MainDllLoader {
 public:
  MainDllLoader();
  virtual ~MainDllLoader();

  // Loads and calls the entry point of chrome.dll. |instance| is the exe
  // instance retrieved from wWinMain.
  // The return value is what the main entry point of chrome.dll returns
  // upon termination.
  int Launch(HINSTANCE instance);

  // Launches a new instance of the browser if the current instance in
  // persistent mode an upgrade is detected.
  void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

 protected:
  // Called after chrome.dll has been loaded but before the entry point
  // is invoked. Derived classes can implement custom actions here.
  // |dll_path| refers to the path of the Chrome dll being loaded.
  virtual void OnBeforeLaunch(const base::string16& dll_path) = 0;

  // Called after the chrome.dll entry point returns and before terminating
  // this process. The return value will be used as the process return code.
  // |dll_path| refers to the path of the Chrome dll being loaded.
  virtual int OnBeforeExit(int return_code, const base::string16& dll_path) = 0;

 private:
  HMODULE Load(base::string16* version, base::string16* out_file);

 private:
  HMODULE dll_;
  std::string process_type_;
  const bool metro_mode_;
};

// Factory for the MainDllLoader. Caller owns the pointer and should call
// delete to free it.
MainDllLoader* MakeMainDllLoader();

#endif  // CHROME_APP_CLIENT_UTIL_H_
