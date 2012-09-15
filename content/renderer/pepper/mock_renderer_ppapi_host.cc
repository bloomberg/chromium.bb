// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/mock_renderer_ppapi_host.h"


namespace content {

MockRendererPpapiHost::MockRendererPpapiHost(RenderView* render_view,
                                             PP_Instance instance)
    : sink_(),
      ppapi_host_(&sink_, ppapi::PpapiPermissions()),
      render_view_(render_view),
      pp_instance_(instance),
      has_user_gesture_(false) {
}

MockRendererPpapiHost::~MockRendererPpapiHost() {
}

ppapi::host::PpapiHost* MockRendererPpapiHost::GetPpapiHost() {
  return &ppapi_host_;
}

bool MockRendererPpapiHost::IsValidInstance(PP_Instance instance) const {
  return instance == pp_instance_;
}

RenderView* MockRendererPpapiHost::GetRenderViewForInstance(
    PP_Instance instance) const {
  if (instance == pp_instance_)
    return render_view_;
  return NULL;
}

bool MockRendererPpapiHost::HasUserGesture(PP_Instance instance) const {
  return has_user_gesture_;
}

}  // namespace content
