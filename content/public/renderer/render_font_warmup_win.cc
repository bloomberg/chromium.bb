// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_font_warmup_win.h"

#include <dwrite.h>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "skia/ext/directwrite_keepalive_win.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace content {

namespace {

struct Unused {};
SkFontMgr* g_warmup_fontmgr = NULL;
base::RepeatingTimer<Unused>* g_keepalive_timer = NULL;

// Windows-only DirectWrite support. These warm up the DirectWrite paths
// before sandbox lock down to allow Skia access to the Font Manager service.
void CreateDirectWriteFactory(IDWriteFactory** factory) {
  typedef decltype(DWriteCreateFactory)* DWriteCreateFactoryProc;
  HMODULE dwrite_dll = LoadLibraryW(L"dwrite.dll");
  // TODO(scottmg): Temporary code to track crash in http://crbug.com/387867.
  if (!dwrite_dll) {
    DWORD load_library_get_last_error = GetLastError();
    base::debug::Alias(&dwrite_dll);
    base::debug::Alias(&load_library_get_last_error);
    CHECK(false);
  }

  DWriteCreateFactoryProc dwrite_create_factory_proc =
      reinterpret_cast<DWriteCreateFactoryProc>(
          GetProcAddress(dwrite_dll, "DWriteCreateFactory"));
  // TODO(scottmg): Temporary code to track crash in http://crbug.com/387867.
  if (!dwrite_create_factory_proc) {
    DWORD get_proc_address_get_last_error = GetLastError();
    base::debug::Alias(&dwrite_create_factory_proc);
    base::debug::Alias(&get_proc_address_get_last_error);
    CHECK(false);
  }
  CHECK(SUCCEEDED(
      dwrite_create_factory_proc(DWRITE_FACTORY_TYPE_ISOLATED,
                                 __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(factory))));
}

}  // namespace

void DoPreSandboxWarmupForTypeface(SkTypeface* typeface) {
  SkPaint paint_warmup;
  paint_warmup.setTypeface(typeface);
  wchar_t glyph = L'S';
  paint_warmup.measureText(&glyph, 2);
}

SkFontMgr* GetPreSandboxWarmupFontMgr() {
  if (!g_warmup_fontmgr) {
    IDWriteFactory* factory;
    CreateDirectWriteFactory(&factory);
    blink::WebFontRendering::setDirectWriteFactory(factory);
    g_warmup_fontmgr = SkFontMgr_New_DirectWrite(factory);
    g_keepalive_timer = new base::RepeatingTimer<Unused>;
    g_keepalive_timer->Start(
        FROM_HERE,
        base::TimeDelta::FromMinutes(10),
        base::Bind(SkiaDirectWriteKeepalive, g_warmup_fontmgr));
  }
  return g_warmup_fontmgr;
}

}  // namespace content
