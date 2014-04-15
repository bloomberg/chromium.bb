// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_font_warmup_win.h"

#include <dwrite.h>

#include "base/logging.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace content {

namespace {

SkFontMgr* g_warmup_fontmgr = NULL;

// Windows-only DirectWrite support. These warm up the DirectWrite paths
// before sandbox lock down to allow Skia access to the Font Manager service.
void CreateDirectWriteFactory(IDWriteFactory** factory) {
  typedef decltype(DWriteCreateFactory)* DWriteCreateFactoryProc;
  DWriteCreateFactoryProc dwrite_create_factory_proc =
      reinterpret_cast<DWriteCreateFactoryProc>(
          GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory"));
  CHECK(dwrite_create_factory_proc);
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
  }
  return g_warmup_fontmgr;
}

}  // namespace content
