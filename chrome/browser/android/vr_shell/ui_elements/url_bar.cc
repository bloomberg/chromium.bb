// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/url_bar.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"

namespace vr_shell {

UrlBar::UrlBar(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<UrlBarTexture>()) {}

UrlBar::~UrlBar() = default;

UiTexture* UrlBar::GetTexture() const {
  return texture_.get();
}

void UrlBar::OnHoverEnter(gfx::PointF position) {
  if (!texture_->SetDrawFlags(UrlBarTexture::FLAG_HOVER))
    return;
  Update();
}

void UrlBar::OnHoverLeave() {
  if (!texture_->SetDrawFlags(0))
    return;
  Update();
}

void UrlBar::OnButtonUp(gfx::PointF position) {
  back_button_callback_.Run();
}

void UrlBar::SetEnabled(bool enabled) {
  if (enabled && !enabled_) {
    Update();
  }
  enabled_ = enabled;
}

void UrlBar::SetURL(const GURL& gurl) {
  // TODO(cjgrant): See if we get duplicate security level numbers, despite the
  // source of this information being called "on changed". Also, consider
  // delaying the texture update slighly, such that back-to-back URL and
  // security state changes generate a single texture update instead of two.
  texture_->SetURL(gurl);
  if (enabled_)
    Update();
}

void UrlBar::SetSecurityLevel(int level) {
  texture_->SetSecurityLevel(level);
  if (enabled_)
    Update();
}

void UrlBar::SetBackButtonCallback(const base::Callback<void()>& callback) {
  back_button_callback_ = callback;
}

}  // namespace vr_shell
