// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/simple_resource_loader.h"

#include <atlbase.h>
#include <string>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win_util.h"

const wchar_t kLocalesDirName[] = L"Locales";

HINSTANCE SimpleResourceLoader::locale_dll_handle_;

SimpleResourceLoader::SimpleResourceLoader() {
  // Find and load the resource DLL.
  std::wstring locale = GetSystemLocale();
  std::wstring locale_dll_path;

  if (GetLocaleFilePath(locale, &locale_dll_path)) {
    DCHECK(locale_dll_handle_ == NULL) << "Locale DLL is already loaded!";
    locale_dll_handle_ = LoadLocaleDll(locale_dll_path);
    DCHECK(locale_dll_handle_ != NULL) << "Failed to load locale dll!";
  }
}


std::wstring SimpleResourceLoader::GetSystemLocale() {
  std::string language, region;
  base::i18n::GetLanguageAndRegionFromOS(&language, &region);
  std::string ret;
  if (!language.empty())
    ret.append(language);
  if (!region.empty()) {
    ret.append("-");
    ret.append(region);
  }
  return ASCIIToWide(ret);
}

bool SimpleResourceLoader::GetLocaleFilePath(const std::wstring& locale,
                                             std::wstring* file_path) {
  DCHECK(file_path);

  FilePath module_path;
  PathService::Get(base::DIR_MODULE, &module_path);
  FilePath locales_path = module_path.Append(kLocalesDirName);

  // We may be residing in the "locales" directory's parent, or we might be
  // in a sibling directory. Move up one and look for Locales again in the
  // latter case.
  if (!file_util::DirectoryExists(locales_path)) {
    locales_path = module_path.DirName();
    locales_path = locales_path.Append(kLocalesDirName);
  }

  bool found_dll = false;
  if (file_util::DirectoryExists(locales_path)) {
    std::wstring dll_name(locale);
    dll_name += L".dll";

    // First look for the named locale DLL.
    FilePath look_path(locales_path.Append(dll_name));
    if (file_util::PathExists(look_path)) {
      *file_path = look_path.value();
      found_dll = true;
    } else {

      // If we didn't find it, try defaulting to en-US.dll.
      dll_name = L"en-US.dll";
      look_path = locales_path.Append(dll_name);
      if (file_util::PathExists(look_path)) {
        *file_path = look_path.value();
        found_dll = true;
      }
    }
  } else {
    NOTREACHED() << "Could not locate locales DLL directory.";
  }

  return found_dll;
}

HINSTANCE SimpleResourceLoader::LoadLocaleDll(const std::wstring& dll_path) {
  DWORD load_flags = 0;
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    load_flags = LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE |
                 LOAD_LIBRARY_AS_IMAGE_RESOURCE;
  } else {
    load_flags = DONT_RESOLVE_DLL_REFERENCES;
  }

  // The dll should only have resources, not executable code.
  HINSTANCE locale_dll_handle = LoadLibraryEx(dll_path.c_str(), NULL,
                                              load_flags);
  DCHECK(locale_dll_handle != NULL) << "unable to load generated resources: "
                                     << GetLastError();

  return locale_dll_handle;
}

std::wstring SimpleResourceLoader::GetLocalizedResource(int message_id) {
  if (!locale_dll_handle_) {
    LOG(WARNING) << "locale resources are not loaded";
    return std::wstring();
  }

  DCHECK(IS_INTRESOURCE(message_id));

  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(
      locale_dll_handle_, message_id);
  if (!image) {
    // Fall back on the current module (shouldn't be any strings here except
    // in unittests).
    image = AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(),
                                      message_id);
    if (!image) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return std::wstring();
    }
  }
  return std::wstring(image->achString, image->nLength);
}

// static
std::wstring SimpleResourceLoader::Get(int message_id) {
  SimpleResourceLoader* loader = SimpleResourceLoader::instance();
  return loader->GetLocalizedResource(message_id);
}

HINSTANCE SimpleResourceLoader::GetResourceModuleHandle() {
  return locale_dll_handle_;
}
