// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

#include "base/trace_event/trace_event.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace vr_shell {

TexturedElement::TexturedElement(int maximum_width)
    : texture_handle_(-1), maximum_width_(maximum_width) {}

TexturedElement::~TexturedElement() = default;

void TexturedElement::Initialize() {
  TRACE_EVENT0("gpu", "TexturedElement::Initialize");
  glGenTextures(1, &texture_handle_);
  DCHECK(GetTexture() != nullptr);
  texture_size_ = GetTexture()->GetPreferredTextureSize(maximum_width_);
  Update();
  set_fill(Fill::SELF);
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  float y = drawn_size.height() / drawn_size.width() * size().x();
  set_size({size().x(), y, 1});
}

void TexturedElement::Update() {
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
      texture_size_.width(), texture_size_.height());
  GetTexture()->DrawAndLayout(surface->getCanvas(), texture_size_);
  Flush(surface.get());
}

void TexturedElement::Render(VrShellRenderer* renderer,
                             vr::Mat4f view_proj_matrix) const {
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  gfx::RectF copy_rect(0, 0, drawn_size.width() / texture_size_.width(),
                       drawn_size.height() / texture_size_.height());
  renderer->GetTexturedQuadRenderer()->AddQuad(
      texture_handle_, view_proj_matrix, copy_rect, opacity());
}

void TexturedElement::Flush(SkSurface* surface) {
  cc::SkiaPaintCanvas paint_canvas(surface->getCanvas());
  paint_canvas.flush();
  SkPixmap pixmap;
  CHECK(surface->peekPixels(&pixmap));

  glBindTexture(GL_TEXTURE_2D, texture_handle_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap.width(), pixmap.height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());
}

}  // namespace vr_shell
