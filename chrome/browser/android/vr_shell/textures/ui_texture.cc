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
constexpr char kDefaultFontFamily[] = "sans-serif";
}  // namespace

// TODO(acondor): Create non-square textures to reduce memory usage.
UITexture::UITexture(int texture_handle, int texture_size)
    : texture_handle_(texture_handle),
      texture_size_(texture_size),
      surface_(SkSurface::MakeRasterN32Premul(texture_size_, texture_size_)) {}

UITexture::~UITexture() = default;

void UITexture::DrawAndLayout() {
  cc::SkiaPaintCanvas paint_canvas(surface_->getCanvas());
  gfx::Canvas canvas(&paint_canvas, 1.0f);
  canvas.DrawColor(0x00000000);
  Draw(&canvas);
  SetSize();
}

void UITexture::Flush() {
  cc::SkiaPaintCanvas paint_canvas(surface_->getCanvas());
  paint_canvas.flush();
  SkPixmap pixmap;
  CHECK(surface_->peekPixels(&pixmap));

  glBindTexture(GL_TEXTURE_2D, texture_handle_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap.width(), pixmap.height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

bool UITexture::IsRTL() {
  return base::i18n::IsRTL();
}

gfx::FontList UITexture::GetFontList(int size, base::string16 text) {
  gfx::Font default_font(kDefaultFontFamily, size);
  std::vector<gfx::Font> fonts{default_font};

  // TODO(acondor): Obtain fallback fonts with gfx::GetFallbackFonts
  // (which is not implemented for android yet) in order to avoid
  // querying per character.

  sk_sp<SkFontMgr> font_mgr(SkFontMgr::RefDefault());
  std::set<std::string> names;
  // TODO(acondor): Query BrowserProcess to obtain the application locale.
  for (base::i18n::UTF16CharIterator it(&text); !it.end(); it.Advance()) {
    sk_sp<SkTypeface> tf(font_mgr->matchFamilyStyleCharacter(
        default_font.GetFontName().c_str(), SkFontStyle(), nullptr, 0,
        it.get()));
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
