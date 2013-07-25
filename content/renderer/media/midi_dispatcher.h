// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_

#include "base/id_map.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebMIDIClient.h"

namespace WebKit {
class WebMIDIPermissionRequest;
}

namespace content {

class RenderViewImpl;

// MIDIDispatcher implements WebMIDIClient to handle permissions for using
// system exclusive messages.
// It works as RenderViewObserver to handle IPC messages between
// MIDIDispatcherHost owned by RenderViewHost since permissions are managed in
// the browser process.
class MIDIDispatcher : public RenderViewObserver,
                       public WebKit::WebMIDIClient {
 public:
  explicit MIDIDispatcher(RenderViewImpl* render_view);
  virtual ~MIDIDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebKit::WebMIDIClient implementation.
  virtual void requestSysExPermission(
      const WebKit::WebMIDIPermissionRequest& request) OVERRIDE;
  virtual void cancelSysExPermissionRequest(
      const WebKit::WebMIDIPermissionRequest& request) OVERRIDE;

  // Permission for using system exclusive messages has been set.
  void OnSysExPermissionApproved(int client_id, bool is_allowed);

  // Each WebMIDIPermissionRequest object is valid until
  // cancelSysExPermissionRequest() is called with the object, or used to call
  // WebMIDIPermissionRequest::setIsAllowed().
  IDMap<WebKit::WebMIDIPermissionRequest> requests_;

  DISALLOW_COPY_AND_ASSIGN(MIDIDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
