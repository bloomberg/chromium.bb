// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_OBSERVER_H_

#include "ipc/ipc_channel.h"
#include "content/common/content_export.h"

class RenderViewHost;
struct ViewMsg_Navigate_Params;

// An observer API implemented by classes which want to filter IPC messages from
// RenderViewHost.
class CONTENT_EXPORT RenderViewHostObserver : public IPC::Channel::Listener,
                                              public IPC::Message::Sender {
 public:

 protected:
  explicit RenderViewHostObserver(RenderViewHost* render_view_host);

  virtual ~RenderViewHostObserver();

  // Invoked after the RenderViewHost is created in the renderer process.  After
  // this point, messages can be sent to it (or to observers in the renderer).
  virtual void RenderViewHostInitialized();

  // Invoked when the RenderViewHost is being destroyed. Gives subclasses a
  // chance to cleanup.  The base implementation will delete the object.
  virtual void RenderViewHostDestroyed();

  // Notifies that a navigation is starting.
  virtual void Navigate(const ViewMsg_Navigate_Params& params);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* message);

  RenderViewHost* render_view_host() const { return render_view_host_; }
  int routing_id() { return routing_id_; }

 private:
  friend class RenderViewHost;

  // Invoked from RenderViewHost. Invokes RenderViewHostDestroyed and NULL out
  // |render_view_host_|.
  void RenderViewHostDestruction();

  RenderViewHost* render_view_host_;

  // The routing ID of the associated RenderViewHost.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostObserver);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_OBSERVER_H_
