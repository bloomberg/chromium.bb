// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "mojo/services/gles2/command_buffer_impl.h"
#include "mojo/services/native_viewport/platform_viewport.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"

namespace ui {
class Event;
}

namespace mojo {

class NativeViewportImpl : public InterfaceImpl<NativeViewport>,
                           public PlatformViewport::Delegate {
 public:
  NativeViewportImpl();
  virtual ~NativeViewportImpl();

  // InterfaceImpl<NativeViewport> implementation.
  virtual void Create(RectPtr bounds) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetBounds(RectPtr bounds) OVERRIDE;
  virtual void CreateGLES2Context(
      InterfaceRequest<CommandBuffer> command_buffer_request) OVERRIDE;

  // PlatformViewport::Delegate implementation.
  virtual void OnBoundsChanged(const gfx::Rect& bounds) OVERRIDE;
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE;
  virtual bool OnEvent(ui::Event* ui_event) OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;

  void AckEvent();
  void CreateCommandBufferIfNeeded();

 private:
  void AckDestroyed();

  gfx::AcceleratedWidget widget_;
  scoped_ptr<PlatformViewport> platform_viewport_;
  InterfaceRequest<CommandBuffer> command_buffer_request_;
  scoped_ptr<CommandBufferImpl> command_buffer_;
  bool waiting_for_event_ack_;
  base::WeakPtrFactory<NativeViewportImpl> weak_factory_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_
