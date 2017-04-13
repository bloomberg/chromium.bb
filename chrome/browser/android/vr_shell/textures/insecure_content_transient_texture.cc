// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/insecure_content_transient_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/vector_icons/vector_icons.h"

namespace vr_shell {

namespace {

const SkColor kBackgroundColor = 0xCC1A1A1A;
const SkColor kForegroundColor = SK_ColorWHITE;

}  // namespace

InsecureContentTransientTexture::InsecureContentTransientTexture(
    int texture_handle,
    int texture_size)
    : UITexture(texture_handle, texture_size) {
  // Ensuring height is half the width.
  DCHECK(texture_size_ % 2 == 0);
  height_ = texture_size_ / 2;
}

InsecureContentTransientTexture::~InsecureContentTransientTexture() = default;

void InsecureContentTransientTexture::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setColor(kBackgroundColor);
  canvas->DrawRoundRect(gfx::Rect(texture_size_, height_), height_ * 0.1,
                        flags);
  canvas->Save();
  canvas->Translate({height_ * 0.1, height_ * 0.25});
  PaintVectorIcon(canvas, ui::kInfoOutlineIcon, height_ * 0.5,
                  kForegroundColor);
  canvas->Restore();
  canvas->Save();
  canvas->Translate({height_ * 0.7, height_ * 0.1});
  // TODO(acondor): Draw text IDS_PAGE_INFO_INSECURE_WEBVR_CONTENT_TRANSIENT.
  canvas->Restore();
}

void InsecureContentTransientTexture::SetSize() {
  size_.SetSize(texture_size_, height_);
}

}  // namespace vr_shell
