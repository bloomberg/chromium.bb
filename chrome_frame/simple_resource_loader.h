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

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
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
  ~SimpleResourceLoader();

  // Retrieves the system language and the region using ICU.
  void GetSystemLocale(std::wstring* language, std::wstring* region);

  // Uses |locale| to build the resource DLL name and then looks for the named
  // DLL in known locales paths. If it doesn't find it, it falls back to
  // looking for an en-US.dll.
  //
  // Returns true if a locale DLL can be found, false otherwise.
  bool GetLocaleFilePath(const std::wstring& language,
                         const std::wstring& region,
                         FilePath* file_path);

  // Loads the locale dll at the given path. Returns a handle to the DLL or
  // NULL on failure.
  HINSTANCE LoadLocaleDll(const FilePath& dll_path);

  // Returns the string resource identified by message_id from the currently
  // loaded locale dll.
  std::wstring GetLocalizedResource(int message_id);

  friend struct DefaultSingletonTraits<SimpleResourceLoader>;

  FRIEND_TEST_ALL_PREFIXES(SimpleResourceLoaderTest, GetLocaleFilePath);

  static HINSTANCE locale_dll_handle_;
};

#endif  // CHROME_FRAME_SIMPLE_RESOURCE_LOADER_H_
