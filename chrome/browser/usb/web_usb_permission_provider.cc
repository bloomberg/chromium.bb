// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_permission_provider.h"

#include "content/public/browser/browser_thread.h"

// static
void WebUSBPermissionProvider::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<PermissionProvider> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);

  // The created object is strongly bound to (and owned by) the pipe.
  new WebUSBPermissionProvider(render_frame_host, request.Pass());
}

WebUSBPermissionProvider::~WebUSBPermissionProvider() {}

WebUSBPermissionProvider::WebUSBPermissionProvider(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<PermissionProvider> request)
    : binding_(this, request.Pass()),
      render_frame_host_(render_frame_host) {}

void WebUSBPermissionProvider::HasDevicePermission(
    mojo::Array<mojo::String> requested_guids,
    const HasDevicePermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(reillyg): Look up permissions granted to the render frame host's
  // current origin in the render frame process's browser context. Until then
  // |render_frame_host_| is unused so this ignore_result() is needed.
  ignore_result(render_frame_host_);
  mojo::Array<mojo::String> allowed_guids(0);
  callback.Run(allowed_guids.Pass());
}
