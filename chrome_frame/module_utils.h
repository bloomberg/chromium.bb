// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_MODULE_UTILS_H_
#define CHROME_FRAME_MODULE_UTILS_H_

#include <ObjBase.h>
#include <windows.h>

class DllRedirector {
 public:
  // Attempts to register a window class under a well known name and appends to
  // its extra data a handle to the current module. Will fail if the window
  // class is already registered. This is intended to be called from DllMain
  // under PROCESS_ATTACH.
  static bool DllRedirector::RegisterAsFirstCFModule();

  // Unregisters the well known window class if we registered it earlier.
  // This is intended to be called from DllMain under PROCESS_DETACH.
  static void DllRedirector::UnregisterAsFirstCFModule();

  // Helper function that extracts the HMODULE parameter from our well known
  // window class.
  static HMODULE GetFirstCFModule();

  // Helper function to return the DllGetClassObject function pointer from
  // the given module. On success, the return value is non-null and module
  // will have had its reference count incremented.
  static LPFNGETCLASSOBJECT GetDllGetClassObjectPtr(HMODULE module);

 private:
  // Use this to keep track of whether or not we have registered the window
  // class in this module.
  static ATOM atom_;

  friend class ModuleUtilsTest;
};

#endif  // CHROME_FRAME_MODULE_UTILS_H_
