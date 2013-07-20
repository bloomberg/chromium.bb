// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBKITPLATFORMSUPPORT_IMPL_H_
#define CONTENT_CHILD_WEBKITPLATFORMSUPPORT_IMPL_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "ui/base/layout.h"
#include "webkit/child/webkitplatformsupport_child_impl.h"

namespace content {

// This is a specialization of WebKitPlatformSupportImpl that implements the
// embedder functions in terms of ContentClient.
class CONTENT_EXPORT WebKitPlatformSupportImpl
    : NON_EXPORTED_BASE(public webkit_glue::WebKitPlatformSupportChildImpl) {
 public:
  typedef WebKit::WebGraphicsContext3D* (OffscreenContextFactory)();

  WebKitPlatformSupportImpl();
  virtual ~WebKitPlatformSupportImpl();

  virtual string16 GetLocalizedString(int message_id) OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) OVERRIDE;
  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info)
      OVERRIDE;
  virtual webkit_glue::WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBKITPLATFORMSUPPORT_IMPL_H_
