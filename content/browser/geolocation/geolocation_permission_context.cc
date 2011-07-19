// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_permission_context.h"

#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/geolocation_messages.h"

// static
void GeolocationPermissionContext::SetGeolocationPermissionResponse(
    int render_process_id, int render_view_id, int bridge_id, bool is_allowed) {
  RenderViewHost* r = RenderViewHost::FromID(render_process_id, render_view_id);
  if (r) {
    r->Send(new GeolocationMsg_PermissionSet(render_view_id, bridge_id,
                                             is_allowed));
  }
}

GeolocationPermissionContext::GeolocationPermissionContext() {}

GeolocationPermissionContext::~GeolocationPermissionContext() {}

