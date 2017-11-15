// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/textured_element.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/skia_surface_provider.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

namespace {
static bool g_initialized_for_testing_ = false;
static bool g_rerender_if_not_dirty_for_testing_ = false;
}

TexturedElement::TexturedElement(int maximum_width)
    : maximum_width_(maximum_width) {}

TexturedElement::~TexturedElement() = default;

void TexturedElement::Initialize(SkiaSurfaceProvider* provider) {
  TRACE_EVENT0("gpu", "TexturedElement::Initialize");
  DCHECK(provider);
  provider_ = provider;
  DCHECK(GetTexture());
  texture_size_ = GetTexture()->GetPreferredTextureSize(maximum_width_);
  GetTexture()->OnInitialized();
  initialized_ = true;
}

// static
void TexturedElement::SetInitializedForTesting() {
  g_initialized_for_testing_ = true;
}

// static
void TexturedElement::SetRerenderIfNotDirtyForTesting() {
  g_rerender_if_not_dirty_for_testing_ = true;
}

bool TexturedElement::UpdateTexture() {
  if (!initialized_ ||
      !(GetTexture()->dirty() || g_rerender_if_not_dirty_for_testing_) ||
      !IsVisible())
    return false;
  surface_ = provider_->MakeSurface(texture_size_);
  DCHECK(surface_.get());
  GetTexture()->DrawAndLayout(surface_->getCanvas(), texture_size_);
  texture_handle_ = provider_->FlushSurface(surface_.get(), texture_handle_);
  // Update the element size's aspect ratio to match the texture.
  UpdateElementSize();
  return true;
}

void TexturedElement::UpdateElementSize() {
  // Updating the height according to width is a hack.  This may be overridden.
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  float height =
      drawn_size.height() / drawn_size.width() * stale_size().width();
  SetSize(stale_size().width(), height);
}

void TexturedElement::Render(UiElementRenderer* renderer,
                             const CameraModel& model) const {
  if (!g_initialized_for_testing_) {
    if (!initialized_)
      return;
    DCHECK(!GetTexture()->dirty());
  }
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  gfx::RectF copy_rect(0, 0, drawn_size.width() / texture_size_.width(),
                       drawn_size.height() / texture_size_.height());
  renderer->DrawTexturedQuad(
      texture_handle_, UiElementRenderer::kTextureLocationLocal,
      model.view_proj_matrix * world_space_transform(), copy_rect,
      computed_opacity(), size(), corner_radius());
}

void TexturedElement::OnSetMode() {
  GetTexture()->SetMode(mode());
}

bool TexturedElement::PrepareToDraw() {
  return UpdateTexture();
}

}  // namespace vr
