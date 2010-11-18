// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for COM objects, error codes etc.

#include "ceee/common/com_utils.h"

#include <atlbase.h>
#include <shlwapi.h>

#include "base/string_util.h"

namespace com {

std::ostream& operator<<(std::ostream& os, const LogHr& hr) {
  // Looks up the human-readable system message for the HRESULT code
  // and since we're not passing any params to FormatMessage, we don't
  // want inserts expanded.
  const DWORD kFlags = FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS;
  char error_text[4096] = { '\0' };
  DWORD message_length = ::FormatMessageA(kFlags, 0, hr.hr_, 0, error_text,
                                          arraysize(error_text), NULL);
  std::string error(error_text);
  TrimString(error, kWhitespaceASCII, &error);

  return os << "[hr=0x" << std::hex << hr.hr_ << ", msg=" << error << "]";
}

std::ostream& operator<<(std::ostream& os, const LogWe& we) {
  // Looks up the human-readable system message for the Windows error code
  // and since we're not passing any params to FormatMessage, we don't
  // want inserts expanded.
  const DWORD kFlags = FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS;
  char error_text[4096] = { '\0' };
  DWORD message_length = ::FormatMessageA(kFlags, 0, we.we_, 0, error_text,
                                          arraysize(error_text), NULL);
  std::string error(error_text);
  TrimString(error, kWhitespaceASCII, &error);

  return os << "[we=" << we.we_ << ", msg=" << error << "]";
}

// Seam so unit tests can mock out the side effects of this function.
HRESULT UpdateRegistryFromResourceImpl(int reg_id, BOOL should_register,
                                       _ATL_REGMAP_ENTRY* entries) {
  return _pAtlModule->UpdateRegistryFromResource(
      reg_id, should_register, entries);
}

HRESULT ModuleRegistrationWithoutAppid(int reg_id, BOOL should_register) {
  // Get our module path.
  wchar_t module_path[MAX_PATH] = { 0 };
  if (0 == ::GetModuleFileName(_AtlBaseModule.GetModuleInstance(),
                               module_path, arraysize(module_path))) {
    NOTREACHED();
    return AlwaysErrorFromLastError();
  }

  // Copy the filename.
  std::wstring module_basename(::PathFindFileName(module_path));
  // And strip the filename.
  if (!::PathRemoveFileSpec(module_path)) {
    NOTREACHED();
    return AlwaysErrorFromLastError();
  }

  _ATL_REGMAP_ENTRY entries[] = {
      { L"MODULE_PATH", module_path },
      { L"MODULE_BASENAME", module_basename.c_str() },
      { NULL, NULL}
    };

  return UpdateRegistryFromResourceImpl(reg_id, should_register, entries);
}

bool GuidToString(const GUID& id, std::wstring* guid_as_text) {
  DCHECK(guid_as_text != NULL);
  if (guid_as_text == NULL)
    return false;

  wchar_t id_as_string[MAX_PATH] = {0};
  if (::StringFromGUID2(id, id_as_string, arraysize(id_as_string)) > 0) {
    *guid_as_text = id_as_string;
    return true;
  }

  NOTREACHED() << "Could not convert GUID to string.";
  return true;
}

}  // namespace com
