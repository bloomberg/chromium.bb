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
class RenderFrame;
}

namespace blink {
class WebFrame;
}

// This proxy is created on the main renderer thread then passed onto
// the blink's worker thread.
class WorkerPermissionClientProxy
    : public blink::WebWorkerPermissionClientProxy {
 public:
  WorkerPermissionClientProxy(content::RenderFrame* render_frame,
                              blink::WebFrame* frame);
  virtual ~WorkerPermissionClientProxy();

  // WebWorkerPermissionClientProxy overrides.
  virtual bool allowDatabase(const blink::WebString& name,
                             const blink::WebString& display_name,
                             unsigned long estimated_size);
  virtual bool requestFileSystemAccessSync();
  virtual bool allowIndexedDB(const blink::WebString& name);

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
