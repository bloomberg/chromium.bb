// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WORKER_PERMISSION_CLIENT_PROXY_H_
#define CHROME_RENDERER_WORKER_PERMISSION_CLIENT_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/web/WebWorkerPermissionClientProxy.h"
#include "url/gurl.h"

namespace IPC {
class SyncMessageFilter;
}

namespace content {
class RenderView;
}

namespace WebKit {
class WebFrame;
}

// This proxy is created on the main renderer thread then passed onto
// the blink's worker thread.
class WorkerPermissionClientProxy
    : public WebKit::WebWorkerPermissionClientProxy {
 public:
  WorkerPermissionClientProxy(content::RenderView* render_view,
                              WebKit::WebFrame* frame);
  virtual ~WorkerPermissionClientProxy();

  // WebWorkerPermissionClientProxy overrides.
  virtual bool allowDatabase(const WebKit::WebString& name,
                             const WebKit::WebString& display_name,
                             unsigned long estimated_size);
  virtual bool allowFileSystem();
  virtual bool allowIndexedDB(const WebKit::WebString& name);

 private:
  // Loading document context for this worker.
  const int routing_id_;
  bool is_unique_origin_;
  GURL document_origin_url_;
  GURL top_frame_origin_url_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(WorkerPermissionClientProxy);
};

#endif  // CHROME_RENDERER_WORKER_PERMISSION_CLIENT_PROXY_H_
