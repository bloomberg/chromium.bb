// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_

#include "content/public/common/socket_permission_request.h"

struct PP_NetAddress_Private;

namespace content {

class RenderViewHost;

namespace pepper_socket_utils {

SocketPermissionRequest CreateSocketPermissionRequest(
    SocketPermissionRequest::OperationType type,
    const PP_NetAddress_Private& net_addr);

bool CanUseSocketAPIs(bool external_plugin,
                      const SocketPermissionRequest& params,
                      RenderViewHost* render_view_host);

}  // namespace pepper_socket_utils

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
