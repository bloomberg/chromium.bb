// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"

#include <set>
#include <string>
#include <vector>

#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

namespace {

static constexpr char kDefaultFontFamily[] = "sans-serif";

}  // namespace

UiTexture::UiTexture() = default;

UiTexture::~UiTexture() = default;

void UiTexture::DrawAndLayout(SkCanvas* canvas, const gfx::Size& texture_size) {
  canvas->drawColor(SK_ColorTRANSPARENT);
  Draw(canvas, texture_size);
}

bool UiTexture::IsRTL() {
  return base::i18n::IsRTL();
}

gfx::FontList UiTexture::GetDefaultFontList(int size) {
  return gfx::FontList(gfx::Font(kDefaultFontFamily, size));
}

gfx::FontList UiTexture::GetFontList(int size, base::string16 text) {
  gfx::Font default_font(kDefaultFontFamily, size);
  std::vector<gfx::Font> fonts{default_font};

  std::set<wchar_t> characters;
  for (base::i18n::UTF16CharIterator it(&text); !it.end(); it.Advance()) {
    characters.insert(it.get());
  }
  // TODO(acondor): Obtain fallback fonts with gfx::GetFallbackFonts
  // (which is not implemented for android yet) in order to avoid
  // querying per character.

  sk_sp<SkFontMgr> font_mgr(SkFontMgr::RefDefault());
  std::set<std::string> names;
  // TODO(acondor): Query BrowserProcess to obtain the application locale.
  for (wchar_t character : characters) {
    sk_sp<SkTypeface> tf(font_mgr->matchFamilyStyleCharacter(
        kDefaultFontFamily, SkFontStyle(), nullptr, 0, character));

    // TODO(acondor): How should we handle no matching font?
    if (!tf)
      continue;
    SkString sk_name;
    tf->getFamilyName(&sk_name);
    std::string name(sk_name.c_str());
    if (name != kDefaultFontFamily)
      names.insert(name);
  }
  for (const auto& name : names)
    fonts.push_back(gfx::Font(name, size));
  return gfx::FontList(fonts);
}

}  // namespace vr_shell
