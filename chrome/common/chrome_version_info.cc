// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "chrome/common/chrome_version_info_posix.h"

// Posix files don't have per-file version information, so we get chrome
// version information from chrome_version_info_posix.h, a generated header.
class ChromeVersionInfoPosix : public FileVersionInfo {
 public:
  ChromeVersionInfoPosix() {}

  virtual std::wstring company_name() { return COMPANY_NAME; }
  virtual std::wstring company_short_name() { return COMPANY_SHORT_NAME; }
  virtual std::wstring product_name() { return PRODUCT_NAME; }
  virtual std::wstring product_short_name() { return PRODUCT_SHORT_NAME; }
  virtual std::wstring internal_name() { return INTERNAL_NAME; }
  virtual std::wstring product_version() { return PRODUCT_VERSION; }
  virtual std::wstring private_build() { return PRIVATE_BUILD; }
  virtual std::wstring special_build() { return SPECIAL_BUILD; }
  virtual std::wstring comments() { return COMMENTS; }
  virtual std::wstring original_filename() { return ORIGINAL_FILENAME; }
  virtual std::wstring file_description() { return FILE_DESCRIPTION; }
  virtual std::wstring file_version() { return FILE_VERSION; }
  virtual std::wstring legal_copyright() { return LEGAL_COPYRIGHT; }
  virtual std::wstring legal_trademarks() { return LEGAL_TRADEMARKS; }
  virtual std::wstring last_change() { return LAST_CHANGE; }
  virtual bool is_official_build() { return OFFICIAL_BUILD; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeVersionInfoPosix);
};
#endif

namespace chrome {

FileVersionInfo* GetChromeVersionInfo() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  return FileVersionInfo::CreateFileVersionInfoForCurrentModule();
#elif defined(OS_POSIX)
  return new ChromeVersionInfoPosix();
#endif
}

}  // namespace chrome
