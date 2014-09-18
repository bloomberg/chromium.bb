// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/media_stream_request.h"
#include "net/http/http_response_headers.h"

#if defined(OS_WIN)
#include "ui/gfx/native_widget_types.h"
#endif

class GURL;

namespace IPC {
class Message;
}

namespace content {
class RenderFrameHost;
class WebContents;
struct AXEventNotificationDetails;
struct ContextMenuParams;
struct TransitionLayerData;

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

  // The RenderFrameHost has been swapped out.
  virtual void SwappedOut(RenderFrameHost* render_frame_host) {}

  // Notification that the navigation on the main frame is blocked waiting
  // for transition to occur.
  virtual void DidDeferAfterResponseStarted(
      const TransitionLayerData& transition_data) {}

  // Used to query whether the navigation transition will be handled.
  virtual bool WillHandleDeferAfterResponseStarted();

  // Notification that a worker process has crashed.
  virtual void WorkerCrashed(RenderFrameHost* render_frame_host) {}

  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) {}

  // A JavaScript message, confirmation or prompt should be shown.
  virtual void RunJavaScriptMessage(RenderFrameHost* render_frame_host,
                                    const base::string16& message,
                                    const base::string16& default_prompt,
                                    const GURL& frame_url,
                                    JavaScriptMessageType type,
                                    IPC::Message* reply_msg) {}

  virtual void RunBeforeUnloadConfirm(RenderFrameHost* render_frame_host,
                                      const base::string16& message,
                                      bool is_reload,
                                      IPC::Message* reply_msg) {}

  // Another page accessed the top-level initial empty document, which means it
  // is no longer safe to display a pending URL without risking a URL spoof.
  virtual void DidAccessInitialDocument() {}

  // The frame set its opener to null, disowning it for the lifetime of the
  // window. Only called for the top-level frame.
  virtual void DidDisownOpener(RenderFrameHost* render_frame_host) {}

  // The onload handler in the frame has completed. Only called for the top-
  // level frame.
  virtual void DocumentOnLoadCompleted(RenderFrameHost* render_frame_host) {}

  // The page's title was changed and should be updated. Only called for the
  // top-level frame.
  virtual void UpdateTitle(RenderFrameHost* render_frame_host,
                           int32 page_id,
                           const base::string16& title,
                           base::i18n::TextDirection title_direction) {}

  // The page's encoding was changed and should be updated. Only called for the
  // top-level frame.
  virtual void UpdateEncoding(RenderFrameHost* render_frame_host,
                              const std::string& encoding) {}

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns NULL.
  virtual WebContents* GetAsWebContents();

  // The render frame has requested access to media devices listed in
  // |request|, and the client should grant or deny that permission by
  // calling |callback|.
  virtual void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback);

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual bool CheckMediaAccessPermission(const GURL& security_origin,
                                          MediaStreamType type);

  // Get the accessibility mode for the WebContents that owns this frame.
  virtual AccessibilityMode GetAccessibilityMode() const;

  // Invoked when an accessibility event is received from the renderer.
  virtual void AccessibilityEventReceived(
      const std::vector<AXEventNotificationDetails>& details) {}

  // Find a guest RenderFrameHost by its browser plugin instance id.
  virtual RenderFrameHost* GetGuestByInstanceID(
      int browser_plugin_instance_id);

#if defined(OS_WIN)
  // Returns the frame's parent's NativeViewAccessible.
  virtual gfx::NativeViewAccessible GetParentNativeViewAccessible();
#endif

 protected:
  virtual ~RenderFrameHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
