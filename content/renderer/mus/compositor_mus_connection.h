// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_
#define CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_

#include "base/bind.h"
#include "base/macros.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class InputHandlerManager;

// CompositorMusConnection manages the connection to the Mandoline UI Service
// (Mus) on the compositor thread. For operations that need to happen on the
// main thread, CompositorMusConnection deals with passing information across
// threads. CompositorMusConnection is constructed on the main thread. By
// default all other methods are assumed to run on the compositor thread unless
// explicited suffixed with OnMainThread.
class CompositorMusConnection
    : public mus::WindowTreeDelegate,
      public mus::InputEventHandler,
      public base::RefCountedThreadSafe<CompositorMusConnection> {
 public:
  // Created on main thread.
  CompositorMusConnection(
      int routing_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request,
      InputHandlerManager* input_handler_manager);

  // Attaches the provided |surface_binding| with the mus::Window for the
  // renderer once it becomes available.
  void AttachSurfaceOnMainThread(
      scoped_ptr<mus::WindowSurfaceBinding> surface_binding);

 private:
  friend class base::RefCountedThreadSafe<CompositorMusConnection>;

  ~CompositorMusConnection() override;

  void AttachSurfaceOnCompositorThread(
      scoped_ptr<mus::WindowSurfaceBinding> surface_binding);

  void CreateWindowTreeConnectionOnCompositorThread(
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request);

  void OnConnectionLostOnMainThread();

  void OnWindowInputEventOnMainThread(
      scoped_ptr<blink::WebInputEvent> web_event,
      const base::Closure& ack);

  void OnWindowInputEventAckOnMainThread(const base::Closure& ack);

  // WindowTreeDelegate implementation:
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;
  void OnEmbed(mus::Window* root) override;

  // InputEventHandler implementation:
  void OnWindowInputEvent(mus::Window* window,
                          mus::mojom::EventPtr event,
                          scoped_ptr<base::Closure>* ack_callback) override;

  const int routing_id_;
  mus::Window* root_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  InputHandlerManager* const input_handler_manager_;
  scoped_ptr<mus::WindowSurfaceBinding> window_surface_binding_;

  DISALLOW_COPY_AND_ASSIGN(CompositorMusConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_
