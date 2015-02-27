// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
#define CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class RenderFrameHost;
class WebContents;
struct Manifest;

// ManifestManagerHost is a helper class that allows callers to get the Manifest
// associated with a frame. It handles the IPC messaging with the child process.
// TODO(mlamouri): keep a cached version and a dirty bit here.
class ManifestManagerHost : public WebContentsObserver {
 public:
  explicit ManifestManagerHost(WebContents* web_contents);
  ~ManifestManagerHost() override;

  typedef base::Callback<void(const Manifest&)> GetManifestCallback;

  // Calls the given callback with the manifest associated with the
  // given RenderFrameHost. If the frame has no manifest or if getting it failed
  // the callback will have an empty manifest.
  void GetManifest(RenderFrameHost*, const GetManifestCallback&);

  // WebContentsObserver
  bool OnMessageReceived(const IPC::Message&, RenderFrameHost*) override;
  void RenderFrameDeleted(RenderFrameHost*) override;

 private:
  typedef IDMap<GetManifestCallback, IDMapOwnPointer> CallbackMap;
  typedef base::hash_map<RenderFrameHost*, CallbackMap*> FrameCallbackMap;

  void OnRequestManifestResponse(
      RenderFrameHost*, int request_id, const Manifest&);

  // Returns the CallbackMap associated with the given RenderFrameHost, or null.
  CallbackMap* GetCallbackMapForFrame(RenderFrameHost*);

  FrameCallbackMap pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ManifestManagerHost);
};

} // namespace content

#endif  // CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
