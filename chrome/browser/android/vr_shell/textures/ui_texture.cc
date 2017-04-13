// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"

#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

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

}  // namespace vr_shell
