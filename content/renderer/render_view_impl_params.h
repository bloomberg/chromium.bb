// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_PARAMS_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_PARAMS_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/common/view_message_enums.h"

namespace WebKit {
struct WebScreenInfo;
}

namespace webkit_glue {
struct WebPreferences;
}

namespace content {

struct RendererPreferences;
typedef base::RefCountedData<int> SharedRenderViewCounter;

// Container for all parameters passed to RenderViewImpl's constructor.
struct CONTENT_EXPORT RenderViewImplParams {
  RenderViewImplParams(int32 opener_id,
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
                       AccessibilityMode accessibility_mode);
  ~RenderViewImplParams();

  int32 opener_id;
  const RendererPreferences& renderer_prefs;
  const webkit_glue::WebPreferences& webkit_prefs;
  SharedRenderViewCounter* counter;
  int32 routing_id;
  int32 surface_id;
  int64 session_storage_namespace_id;
  const string16& frame_name;
  bool is_renderer_created;
  bool swapped_out;
  int32 next_page_id;
  const WebKit::WebScreenInfo& screen_info;
  AccessibilityMode accessibility_mode;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_PARAMS_H_
