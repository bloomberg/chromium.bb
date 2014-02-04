// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_

#include "base/id_map.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebMIDIClient.h"

namespace blink {
class WebMIDIPermissionRequest;
}

namespace content {

class RenderViewImpl;

// MidiDispatcher implements WebMIDIClient to handle permissions for using
// system exclusive messages.
// It works as RenderViewObserver to handle IPC messages between
// MidiDispatcherHost owned by RenderViewHost since permissions are managed in
// the browser process.
class MidiDispatcher : public RenderViewObserver,
                       public blink::WebMIDIClient {
 public:
  explicit MidiDispatcher(RenderViewImpl* render_view);
  virtual ~MidiDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // blink::WebMIDIClient implementation.
  virtual void requestSysExPermission(
      const blink::WebMIDIPermissionRequest& request) OVERRIDE;
  virtual void cancelSysExPermissionRequest(
      const blink::WebMIDIPermissionRequest& request) OVERRIDE;

  // Permission for using system exclusive messages has been set.
  void OnSysExPermissionApproved(int client_id, bool is_allowed);

  // Each WebMIDIPermissionRequest object is valid until
  // cancelSysExPermissionRequest() is called with the object, or used to call
  // WebMIDIPermissionRequest::setIsAllowed().
  IDMap<blink::WebMIDIPermissionRequest> requests_;

  DISALLOW_COPY_AND_ASSIGN(MidiDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
