// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_
#define CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebContentSettingsClient.h"
#include "url/gurl.h"

namespace IPC {
class SyncMessageFilter;
}

namespace content {
class RenderFrame;
}

namespace blink {
class WebSecurityOrigin;
}

// This client is created on the main renderer thread then passed onto the
// blink's worker thread.
class WorkerContentSettingsClient : public blink::WebContentSettingsClient {
 public:
  explicit WorkerContentSettingsClient(content::RenderFrame* render_frame);
  ~WorkerContentSettingsClient() override;

  // WebContentSettingsClient overrides.
  bool RequestFileSystemAccessSync() override;
  bool AllowIndexedDB(const blink::WebString& name,
                      const blink::WebSecurityOrigin&) override;
  bool AllowRunningInsecureContent(bool allowed_per_settings,
                                   const blink::WebSecurityOrigin& context,
                                   const blink::WebURL& url) override;

 private:
  // Loading document context for this worker.
  const int routing_id_;
  bool is_unique_origin_;
  GURL document_origin_url_;
  GURL top_frame_origin_url_;
  bool allow_running_insecure_content_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(WorkerContentSettingsClient);
};

#endif  // CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_
