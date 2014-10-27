// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MIDI_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_MEDIA_MIDI_DISPATCHER_HOST_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class BrowserContext;

// MidiDispatcherHost handles permissions for using system exclusive messages.
class MidiDispatcherHost : public WebContentsObserver {
 public:
  explicit MidiDispatcherHost(WebContents* web_contents);
  ~MidiDispatcherHost() override;

  // WebContentsObserver implementation.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

 private:
  void OnRequestSysExPermission(RenderFrameHost* render_frame_host,
                                int bridge_id,
                                const GURL& origin,
                                bool user_gesture);
  void OnCancelSysExPermissionRequest(RenderFrameHost* render_frame_host,
                                      int bridge_id,
                                      const GURL& requesting_frame);
  void WasSysExPermissionGranted(int render_process_id,
                                 int render_frame_id,
                                 int bridge_id,
                                 bool is_allowed);
  void CancelPermissionRequestsForFrame(RenderFrameHost* render_frame_host);

  struct PendingPermission {
    PendingPermission(int render_process_id,
                      int render_frame_id,
                      int bridge_id);
    ~PendingPermission();
    int render_process_id;
    int render_frame_id;
    int bridge_id;
  };
  std::vector<PendingPermission> pending_permissions_;

  base::WeakPtrFactory<MidiDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MidiDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MIDI_DISPATCHER_HOST_H_
