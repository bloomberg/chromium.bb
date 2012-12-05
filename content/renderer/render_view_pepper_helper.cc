// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_pepper_helper.h"

#include "ui/gfx/rect.h"

namespace content {

RenderViewPepperHelper::~RenderViewPepperHelper() {
}

WebKit::WebPlugin* RenderViewPepperHelper::CreatePepperWebPlugin(
    const webkit::WebPluginInfo& webplugin_info,
    const WebKit::WebPluginParams& params) {
  return NULL;
}

webkit::ppapi::PluginInstance*
RenderViewPepperHelper::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip,
    float* scale_factor) {
  return NULL;
}

bool RenderViewPepperHelper::IsPluginFocused() const {
  return false;
}

gfx::Rect RenderViewPepperHelper::GetCaretBounds() const {
  return gfx::Rect(0, 0, 0, 0);
}

ui::TextInputType RenderViewPepperHelper::GetTextInputType() const {
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool RenderViewPepperHelper::IsPluginAcceptingCompositionEvents() const {
  return false;
}

bool RenderViewPepperHelper::CanComposeInline() const {
  return false;
}

}  // namespace content
