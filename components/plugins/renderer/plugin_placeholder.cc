// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/plugin_placeholder.h"

#include "content/public/renderer/render_frame.h"

namespace plugins {

gin::WrapperInfo PluginPlaceholder::kWrapperInfo = {gin::kEmbedderNativeGin};

PluginPlaceholder::PluginPlaceholder(content::RenderFrame* render_frame,
                                     blink::WebLocalFrame* frame,
                                     const blink::WebPluginParams& params,
                                     const std::string& html_data,
                                     GURL placeholderDataUrl)
    : content::RenderFrameObserver(render_frame),
      frame_(frame),
      plugin_params_(params),
      plugin_(WebViewPlugin::Create(this,
                                    render_frame->GetWebkitPreferences(),
                                    html_data,
                                    placeholderDataUrl)) {
  DCHECK(placeholderDataUrl.is_valid())
      << "Blink requires the placeholder to have a valid URL.";
}

PluginPlaceholder::~PluginPlaceholder() {}

const blink::WebPluginParams& PluginPlaceholder::GetPluginParams() const {
  return plugin_params_;
}

void PluginPlaceholder::ShowContextMenu(const blink::WebMouseEvent& event) {
  // Does nothing by default. Will be overridden if a specific browser wants
  // a context menu.
  return;
}

void PluginPlaceholder::PluginDestroyed() {
  plugin_ = NULL;
}

void PluginPlaceholder::OnDestruct() {
  frame_ = NULL;
}

blink::WebLocalFrame* PluginPlaceholder::GetFrame() { return frame_; }

}  // namespace plugins
