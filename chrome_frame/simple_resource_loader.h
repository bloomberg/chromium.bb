// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class implements a simplified and much stripped down version of
// Chrome's app/resource_bundle machinery. It notably avoids unwanted
// dependencies on things like Skia.
//

#ifndef CHROME_FRAME_SIMPLE_RESOURCE_LOADER_H_
#define CHROME_FRAME_SIMPLE_RESOURCE_LOADER_H_

#include <windows.h>
#include <string>

#include "base/singleton.h"

class SimpleResourceLoader {
 public:

  static SimpleResourceLoader* instance() {
    return Singleton<SimpleResourceLoader>::get();
  }

  // Helper method to return the string resource identified by message_id
  // from the currently loaded locale dll.
  static std::wstring Get(int message_id);

  // Retrieves the HINSTANCE of the loaded module handle. May be NULL if a
  // resource DLL could not be loaded.
  HINSTANCE GetResourceModuleHandle();

 private:
  SimpleResourceLoader();

  // Retrieves the system locale in the <language>-<region> format using ICU.
  std::wstring GetSystemLocale();

  // Uses |locale| to build the resource DLL name and then looks for the named
  // DLL in known locales paths. If it doesn't find it, it falls back to
  // looking for an en-US.dll.
  //
  // Returns true if a locale DLL can be found, false otherwise.
  bool GetLocaleFilePath(const std::wstring& locale, std::wstring* file_path);

  // Loads the locale dll at the given path. Returns a handle to the DLL or
  // NULL on failure.
  HINSTANCE LoadLocaleDll(const std::wstring& dll_path);

  // Returns the string resource identified by message_id from the currently
  // loaded locale dll.
  std::wstring GetLocalizedResource(int message_id);

  friend struct DefaultSingletonTraits<SimpleResourceLoader>;

  static HINSTANCE locale_dll_handle_;
};

#endif  // CHROME_FRAME_SIMPLE_RESOURCE_LOADER_H_
