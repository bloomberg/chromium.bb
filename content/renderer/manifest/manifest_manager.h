// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MANIFEST_MANIFEST_MANAGER_H_
#define CONTENT_RENDERER_MANIFEST_MANIFEST_MANAGER_H_

#include <list>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/manifest.h"
#include "content/public/renderer/render_frame_observer.h"

class GURL;

namespace blink {
class WebURLResponse;
}

namespace content {

class ManifestFetcher;

// The ManifestManager is a helper class that takes care of fetching and parsing
// the Manifest of the associated RenderFrame. It uses the ManifestFetcher and
// the ManifestParser in order to do so.
// There are two expected consumers of this helper: ManifestManagerHost, via IPC
// messages and callers inside the renderer process. The latter should use
// GetManifest().
class ManifestManager : public RenderFrameObserver {
 public:
  typedef base::Callback<void(const Manifest&)> GetManifestCallback;

  explicit ManifestManager(RenderFrame* render_frame);
  virtual ~ManifestManager();

  // Will call the given |callback| with the Manifest associated with the
  // RenderFrame if any. Will pass an empty Manifest in case of error.
  void GetManifest(const GetManifestCallback& callback);

  // RenderFrameObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidChangeManifest() OVERRIDE;

 private:
  enum ResolveState {
    ResolveStateSuccess,
    ResolveStateFailure
  };

  // Called when receiving a ManifestManagerMsg_RequestManifest from the browser
  // process.
  void OnRequestManifest(int request_id);
  void OnRequestManifestComplete(int request_id, const Manifest&);

  void FetchManifest();
  void OnManifestFetchComplete(const GURL& document_url,
                               const blink::WebURLResponse& response,
                               const std::string& data);
  void ResolveCallbacks(ResolveState state);

  scoped_ptr<ManifestFetcher> fetcher_;

  // Whether the RenderFrame may have an associated Manifest. If true, the frame
  // may have a manifest, if false, it can't have one. This boolean is true when
  // DidChangeManifest() is called, if it is never called, it means that the
  // associated document has no <link rel='manifest'>.
  bool may_have_manifest_;

  // Whether the current Manifest is dirty.
  bool manifest_dirty_;

  // Current Manifest. Might be outdated if manifest_dirty_ is true.
  Manifest manifest_;

  std::list<GetManifestCallback> pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ManifestManager);
};

} // namespace content

#endif // CONTENT_RENDERER_MANIFEST_MANIFEST_MANAGER_H_
