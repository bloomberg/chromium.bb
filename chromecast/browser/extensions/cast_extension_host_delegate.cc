// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extension_host_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"
#include "extensions/browser/serial_extension_host_queue.h"

namespace extensions {

CastExtensionHostDelegate::CastExtensionHostDelegate() {}

CastExtensionHostDelegate::~CastExtensionHostDelegate() {}

void CastExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {
  CastExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

void CastExtensionHostDelegate::OnRenderViewCreatedForBackgroundPage(
    ExtensionHost* host) {}

content::JavaScriptDialogManager*
CastExtensionHostDelegate::GetJavaScriptDialogManager() {
  NOTREACHED();
  return nullptr;
}

void CastExtensionHostDelegate::CreateTab(content::WebContents* web_contents,
                                          const std::string& extension_id,
                                          WindowOpenDisposition disposition,
                                          const gfx::Rect& initial_rect,
                                          bool user_gesture) {
  NOTREACHED();
}

void CastExtensionHostDelegate::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const Extension* extension) {}

bool CastExtensionHostDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    content::MediaStreamType type,
    const Extension* extension) {
  return true;
}

static base::LazyInstance<SerialExtensionHostQueue>::DestructorAtExit g_queue =
    LAZY_INSTANCE_INITIALIZER;

ExtensionHostQueue* CastExtensionHostDelegate::GetExtensionHostQueue() const {
  return g_queue.Pointer();
}

}  // namespace extensions
