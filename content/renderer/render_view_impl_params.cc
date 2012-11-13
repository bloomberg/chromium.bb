// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl_params.h"

namespace content {

RenderViewImplParams::RenderViewImplParams(
    int32 opener_id,
    const RendererPreferences& renderer_prefs,
    const webkit_glue::WebPreferences& webkit_prefs,
    SharedRenderViewCounter* counter,
    int32 routing_id,
    int32 surface_id,
    int64 session_storage_namespace_id,
    const string16& frame_name,
    bool is_renderer_created,
    bool swapped_out,
    int32 next_page_id,
    const WebKit::WebScreenInfo& screen_info,
    AccessibilityMode accessibility_mode)
    : opener_id(opener_id),
      renderer_prefs(renderer_prefs),
      webkit_prefs(webkit_prefs),
      counter(counter),
      routing_id(routing_id),
      surface_id(surface_id),
      session_storage_namespace_id(session_storage_namespace_id),
      frame_name(frame_name),
      is_renderer_created(is_renderer_created),
      swapped_out(swapped_out),
      next_page_id(next_page_id),
      screen_info(screen_info),
      accessibility_mode(accessibility_mode) {
}

RenderViewImplParams::~RenderViewImplParams() {}

}  // namespace content
