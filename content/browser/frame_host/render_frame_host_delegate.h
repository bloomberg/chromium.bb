// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {
class RenderFrameHost;
class WebContents;
struct ContextMenuParams;

// An interface implemented by an object interested in knowing about the state
// of the RenderFrameHost.
class CONTENT_EXPORT RenderFrameHostDelegate {
 public:
  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderFrameHost* render_frame_host,
                                 const IPC::Message& message);

  // Gets the last committed URL. See WebContents::GetLastCommittedURL for a
  // description of the semantics.
  virtual const GURL& GetMainFrameLastCommittedURL() const;

  // A message was added to to the console.
  virtual bool AddMessageToConsole(int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id);

  // Informs the delegate whenever a RenderFrameHost is created.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Informs the delegate whenever a RenderFrameHost is deleted.
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) {}

  // The top-level RenderFrame began loading a new page. This corresponds to
  // Blink's notion of the throbber starting.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  virtual void DidStartLoading(RenderFrameHost* render_frame_host,
                               bool to_different_document) {}

  // The top-level RenderFrame stopped loading a page. This corresponds to
  // Blink's notion of the throbber stopping.
  virtual void DidStopLoading(RenderFrameHost* render_frame_host) {}

  // The RenderFrameHost has been swapped out.
  virtual void SwappedOut(RenderFrameHost* render_frame_host) {}

  // Notification that a worker process has crashed.
  virtual void WorkerCrashed(RenderFrameHost* render_frame_host) {}

  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) {}

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns NULL.
  virtual WebContents* GetAsWebContents();

 protected:
  virtual ~RenderFrameHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
