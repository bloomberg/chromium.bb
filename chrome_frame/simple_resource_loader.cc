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
  std::wstring language;
  std::wstring region;
  GetSystemLocale(&language, &region);
  FilePath locale_dll_path;

  if (GetLocaleFilePath(language, region, &locale_dll_path)) {
    DCHECK(locale_dll_handle_ == NULL) << "Locale DLL is already loaded!";
    locale_dll_handle_ = LoadLocaleDll(locale_dll_path);
    DCHECK(locale_dll_handle_ != NULL) << "Failed to load locale dll!";
  }
}

SimpleResourceLoader::~SimpleResourceLoader() {}

void SimpleResourceLoader::GetSystemLocale(std::wstring* language,
                                           std::wstring* region) {
  DCHECK(language);
  DCHECK(region);

  std::string icu_language, icu_region;
  base::i18n::GetLanguageAndRegionFromOS(&icu_language, &icu_region);
  if (!icu_language.empty()) {
    *language = ASCIIToWide(icu_language);
  }
  if (!icu_region.empty()) {
    *region = ASCIIToWide(icu_region);
  }
}

bool SimpleResourceLoader::GetLocaleFilePath(const std::wstring& language,
                                             const std::wstring& region,
                                             FilePath* file_path) {
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
    std::wstring dll_name(language);
    FilePath look_path;

    // First look for the [language]-[region].DLL.
    if (!region.empty()) {
      dll_name += L"-";
      dll_name += region;
      dll_name += L".dll";

      look_path = locales_path.Append(dll_name);
      if (file_util::PathExists(look_path)) {
        *file_path = look_path;
        found_dll = true;
      }
    }

    // Next look for just [language].DLL.
    if (!found_dll) {
      dll_name = language;
      dll_name += L".dll";
      look_path = locales_path.Append(dll_name);
      if (file_util::PathExists(look_path)) {
        *file_path = look_path;
        found_dll = true;
      }
    }

    // Finally, try defaulting to en-US.dll.
    if (!found_dll) {
      look_path = locales_path.Append(L"en-US.dll");
      if (file_util::PathExists(look_path)) {
        *file_path = look_path;
        found_dll = true;
      }
    }
  } else {
    NOTREACHED() << "Could not locate locales DLL directory.";
  }

  return found_dll;
}

HINSTANCE SimpleResourceLoader::LoadLocaleDll(const FilePath& dll_path) {
  DWORD load_flags = 0;
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    load_flags = LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE |
                 LOAD_LIBRARY_AS_IMAGE_RESOURCE;
  } else {
    load_flags = DONT_RESOLVE_DLL_REFERENCES;
  }

  // The dll should only have resources, not executable code.
  HINSTANCE locale_dll_handle = LoadLibraryEx(dll_path.value().c_str(), NULL,
                                              load_flags);
  DCHECK(locale_dll_handle != NULL) << "unable to load generated resources: "
                                     << GetLastError();

  return locale_dll_handle;
}

std::wstring SimpleResourceLoader::GetLocalizedResource(int message_id) {
  if (!locale_dll_handle_) {
    DLOG(ERROR) << "locale resources are not loaded";
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
