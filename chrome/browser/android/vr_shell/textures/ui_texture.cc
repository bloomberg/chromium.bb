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
#include "chrome/browser/android/vr_shell/font_fallback.h"
#include "chrome/browser/browser_process.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

namespace {

static constexpr char kDefaultFontFamily[] = "sans-serif";

std::set<UChar32> CollectDifferentChars(base::string16 text) {
  std::set<UChar32> characters;
  for (base::i18n::UTF16CharIterator it(&text); !it.end(); it.Advance()) {
    characters.insert(it.get());
  }
  return characters;
}

}  // namespace

UiTexture::UiTexture() = default;

UiTexture::~UiTexture() = default;

bool UiTexture::SetDrawFlags(int draw_flags) {
  if (draw_flags == draw_flags_)
    return false;
  draw_flags_ = draw_flags;
  return true;
}

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

  std::set<std::string> names;
  // TODO(acondor): Query BrowserProcess to obtain the application locale.
  for (UChar32 c : CollectDifferentChars(text)) {
    std::string name = GetFallbackFontNameForChar(default_font, c, "");
    if (!name.empty())
      names.insert(name);
  }
  for (const auto& name : names)
    fonts.push_back(gfx::Font(name, size));
  return gfx::FontList(fonts);
}

}  // namespace vr_shell
