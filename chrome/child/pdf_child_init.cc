// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/pdf_child_init.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/win/current_module.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#endif

namespace {

#if defined(OS_WIN)
typedef decltype(::GetFontData)* GetFontDataPtr;
GetFontDataPtr g_original_get_font_data = nullptr;


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
      if (content::ChildThread::Get())
        content::ChildThread::Get()->PreCacheFont(logfont);
      rv = g_original_get_font_data(hdc, table, offset, buffer, length);
      if (content::ChildThread::Get())
        content::ChildThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}
#endif  // defined(OS_WIN)

}  // namespace

void MaybeInitializeGDI() {
#if defined(OS_WIN)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  // Patch utility processes which explicitly need GDI. Anything else, just
  // return.
  service_manager::SandboxType service_sandbox_type =
      service_manager::SandboxTypeFromCommandLine(command_line);
  if (!(service_sandbox_type == service_manager::SandboxType::kPpapi ||
        service_sandbox_type ==
            service_manager::SandboxType::kPrintCompositor ||
        service_sandbox_type == service_manager::SandboxType::kPdfConversion)) {
    return;
  }

#if defined(COMPONENT_BUILD)
  HMODULE module = ::GetModuleHandleA("pdfium.dll");
  DCHECK(module);
#else
  HMODULE module = CURRENT_MODULE();
#endif  // defined(COMPONENT_BUILD)

  // Need to patch GetFontData() for font loading to work correctly. This can be
  // removed once PDFium switches to use Skia. https://crbug.com/pdfium/11
  static base::NoDestructor<base::win::IATPatchFunction> patch_get_font_data;
  patch_get_font_data->PatchFromModule(
      module, "gdi32.dll", "GetFontData",
      reinterpret_cast<void*>(GetFontDataPatch));
  g_original_get_font_data = reinterpret_cast<GetFontDataPtr>(
      patch_get_font_data->original_function());
#endif  // defined(OS_WIN)
}
