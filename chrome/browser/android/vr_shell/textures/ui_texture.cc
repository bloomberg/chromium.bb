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
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/vr_shell/font_fallback.h"
#include "chrome/browser/browser_process.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

namespace {

static constexpr char kDefaultFontFamily[] = "sans-serif";

std::unique_ptr<gfx::RenderText> CreateRenderText(
    const base::string16& text,
    const gfx::FontList& font_list,
    SkColor color,
    int flags) {
  std::unique_ptr<gfx::RenderText> render_text(
      gfx::RenderText::CreateInstance());
  render_text->SetText(text);
  render_text->SetFontList(font_list);
  render_text->SetColor(color);

  if (flags & UiTexture::TEXT_ALIGN_LEFT)
    render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  else if (flags & UiTexture::TEXT_ALIGN_RIGHT)
    render_text->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  else if (flags & UiTexture::TEXT_ALIGN_CENTER)
    render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  const int font_style = font_list.GetFontStyle();
  render_text->SetStyle(gfx::ITALIC, (font_style & gfx::Font::ITALIC) != 0);
  render_text->SetStyle(gfx::UNDERLINE,
                        (font_style & gfx::Font::UNDERLINE) != 0);
  render_text->SetWeight(font_list.GetFontWeight());

  return render_text;
}

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
  TRACE_EVENT0("gpu", "UiTexture::DrawAndLayout");
  canvas->drawColor(SK_ColorTRANSPARENT);
  Draw(canvas, texture_size);
}

std::vector<std::unique_ptr<gfx::RenderText>> UiTexture::PrepareDrawStringRect(
    const base::string16& text,
    const gfx::FontList& font_list,
    SkColor color,
    gfx::Rect* bounds,
    int flags) {
  DCHECK(bounds);

  std::vector<std::unique_ptr<gfx::RenderText>> lines;
  gfx::Rect rect(*bounds);

  if (flags & MULTI_LINE) {
    std::vector<base::string16> strings;
    gfx::ElideRectangleText(text, font_list, bounds->width(),
                            bounds->height() ? bounds->height() : INT_MAX,
                            gfx::WRAP_LONG_WORDS, &strings);

    int height = 0;
    int line_height = 0;
    for (size_t i = 0; i < strings.size(); i++) {
      std::unique_ptr<gfx::RenderText> render_text =
          CreateRenderText(strings[i], font_list, color, flags);

      if (i == 0) {
        // Measure line and center text vertically.
        line_height = render_text->GetStringSize().height();
        rect.set_height(line_height);
        if (bounds->height()) {
          const int text_height = strings.size() * line_height;
          rect += gfx::Vector2d(0, (bounds->height() - text_height) / 2);
        }
      }

      render_text->SetDisplayRect(rect);
      height += line_height;
      rect += gfx::Vector2d(0, line_height);
      lines.push_back(std::move(render_text));
    }

    // Set calculated height.
    if (bounds->height() == 0)
      bounds->set_height(height);

  } else {
    std::unique_ptr<gfx::RenderText> render_text =
        CreateRenderText(text, font_list, color, flags);
    if (bounds->width() != 0)
      render_text->SetElideBehavior(gfx::TRUNCATE);
    else
      rect.set_width(INT_MAX);

    render_text->SetDisplayRect(rect);

    if (bounds->width() == 0) {
      int text_width = render_text->GetStringSize().width();
      bounds->set_width(text_width);
      rect.set_width(text_width);
      render_text->SetDisplayRect(rect);
    }

    lines.push_back(std::move(render_text));
  }
  return lines;
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
