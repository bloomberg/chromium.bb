// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions that can report details about the
// host operating environment.

#ifndef CHROME_APP_CLIENT_UTIL_H_
#define CHROME_APP_CLIENT_UTIL_H_
#pragma once

#include <windows.h>
#include <string>

namespace sandbox {
  struct SandboxInterfaceInfo;
}

// Implements the common aspects of loading chrome.dll for both chrome and
// chromium scenarios, which are in charge of implementing two abstract
// methods: GetRegistryPath() and OnBeforeLaunch().
class MainDllLoader {
 public:
  MainDllLoader();
  virtual ~MainDllLoader();

  // Loads and calls the entry point of chrome.dll. |instance| is the exe
  // instance retrieved from wWinMain and the |sbox_info| is the broker or
  // target services interface pointer.
  // The return value is what the main entry point of chrome.dll returns
  // upon termination.
  int Launch(HINSTANCE instance, sandbox::SandboxInterfaceInfo* sbox_info);

  // Launches a new instance of the browser if the current instance in
  // persistent mode an upgrade is detected.
  void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

  // Derived classes must return the relative registry path that holds the
  // most current version of chrome.dll.
  virtual std::wstring GetRegistryPath() = 0;

  // Called after chrome.dll has been loaded but before the entry point
  // is invoked. Derived classes can implement custom actions here.
  virtual void OnBeforeLaunch() {}

  // Called after the chrome.dll entry point returns and before terminating
  // this process. The return value will be used as the process return code.
  virtual int OnBeforeExit(int return_code) { return return_code; }

 protected:
  HMODULE Load(std::wstring* out_version, std::wstring* out_file);

 private:
  // Chrome.dll handle.
  HMODULE dll_;
};

// Factory for the MainDllLoader. Caller owns the pointer and should call
// delete to free it.
MainDllLoader* MakeMainDllLoader();

#endif  // CHROME_APP_CLIENT_UTIL_H_
