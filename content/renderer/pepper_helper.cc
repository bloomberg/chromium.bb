// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_helper.h"

#include "ui/gfx/rect.h"

namespace content {

PepperHelper::~PepperHelper() {
}

WebKit::WebPlugin* PepperHelper::CreatePepperWebPlugin(
    const WebPluginInfo& webplugin_info,
    const WebKit::WebPluginParams& params) {
  return NULL;
}

PepperPluginInstanceImpl* PepperHelper::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip,
    float* scale_factor) {
  return NULL;
}

bool PepperHelper::IsPluginFocused() const {
  return false;
}

gfx::Rect PepperHelper::GetCaretBounds() const {
  return gfx::Rect(0, 0, 0, 0);
}

ui::TextInputType PepperHelper::GetTextInputType() const {
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool PepperHelper::IsPluginAcceptingCompositionEvents() const {
  return false;
}

bool PepperHelper::CanComposeInline() const {
  return false;
}

}  // namespace content
