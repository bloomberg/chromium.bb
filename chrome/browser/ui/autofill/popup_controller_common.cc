// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_controller_common.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

PopupControllerCommon::PopupControllerCommon(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const gfx::NativeView container_view,
    content::WebContents* web_contents)
    : element_bounds_(element_bounds),
      text_direction_(text_direction),
      container_view_(container_view),
      web_contents_(web_contents),
      key_press_event_target_(NULL) {
}
PopupControllerCommon::~PopupControllerCommon() {}

void PopupControllerCommon::SetKeyPressCallback(
    content::RenderWidgetHost::KeyPressEventCallback callback) {
  DCHECK(key_press_event_callback_.is_null());
  key_press_event_callback_ = callback;
}

void PopupControllerCommon::RegisterKeyPressCallback() {
  if (web_contents_ && !key_press_event_target_) {
    key_press_event_target_ = web_contents_->GetRenderViewHost();
    key_press_event_target_->GetWidget()->AddKeyPressEventCallback(
        key_press_event_callback_);
  }
}

void PopupControllerCommon::RemoveKeyPressCallback() {
  if (web_contents_ && (!web_contents_->IsBeingDestroyed()) &&
      key_press_event_target_ == web_contents_->GetRenderViewHost()) {
    web_contents_->GetRenderViewHost()
        ->GetWidget()
        ->RemoveKeyPressEventCallback(key_press_event_callback_);
  }
  key_press_event_target_ = NULL;
}

}  // namespace autofill
