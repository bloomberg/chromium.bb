// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/pdf_child_init.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/child/child_thread.h"

#if defined(OS_WIN)
#include "base/win/iat_patch_function.h"
#endif

namespace chrome {
namespace {
#if defined(OS_WIN)
static base::win::IATPatchFunction g_iat_patch_createdca;
HDC WINAPI CreateDCAPatch(LPCSTR driver_name,
                          LPCSTR device_name,
                          LPCSTR output,
                          const void* init_data) {
  DCHECK(std::string("DISPLAY") == std::string(driver_name));
  DCHECK(!device_name);
  DCHECK(!output);
  DCHECK(!init_data);

  // CreateDC fails behind the sandbox, but not CreateCompatibleDC.
  return CreateCompatibleDC(NULL);
}

typedef DWORD (WINAPI* GetFontDataPtr) (HDC hdc,
                                        DWORD table,
                                        DWORD offset,
                                        LPVOID buffer,
                                        DWORD length);
GetFontDataPtr g_original_get_font_data = NULL;
static base::win::IATPatchFunction g_iat_patch_get_font_data;
DWORD WINAPI GetFontDataPatch(HDC hdc,
                              DWORD table,
                              DWORD offset,
                              LPVOID buffer,
                              DWORD length) {
  DWORD rv = g_original_get_font_data(hdc, table, offset, buffer, length);
  if (rv == GDI_ERROR && hdc) {
    HFONT font = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));

    LOGFONT logfont;
    if (GetObject(font, sizeof(LOGFONT), &logfont)) {
      std::vector<char> font_data;
      if (content::ChildThread::Get())
        content::ChildThread::Get()->PreCacheFont(logfont);
      rv = g_original_get_font_data(hdc, table, offset, buffer, length);
      if (content::ChildThread::Get())
        content::ChildThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}
#endif  // OS_WIN

}  // namespace

#if defined(OS_WIN)
// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif  // OS_WIN

void InitializePDF() {
#if defined(OS_WIN)
  // Need to patch a few functions for font loading to work correctly. This can
  // be removed once we switch PDF to use Skia.
  HMODULE current_module = reinterpret_cast<HMODULE>(&__ImageBase);
  g_iat_patch_createdca.PatchFromModule(current_module, "gdi32.dll",
                                        "CreateDCA", CreateDCAPatch);
  g_iat_patch_get_font_data.PatchFromModule(current_module, "gdi32.dll",
                                            "GetFontData", GetFontDataPatch);
  g_original_get_font_data = reinterpret_cast<GetFontDataPtr>(
      g_iat_patch_get_font_data.original_function());
#endif  // OS_WIN
}

}  // namespace chrome
