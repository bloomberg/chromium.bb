// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NATIVE_VIEWPORT_NATIVE_VIEWPORT_IMPL_H_
#define COMPONENTS_NATIVE_VIEWPORT_NATIVE_VIEWPORT_IMPL_H_

#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/gpu/public/interfaces/gpu.mojom.h"
#include "components/native_viewport/onscreen_context_provider.h"
#include "components/native_viewport/platform_viewport.h"
#include "components/native_viewport/public/interfaces/native_viewport.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gfx/geometry/rect.h"

namespace gles2 {
class GpuState;
}

namespace ui {
class Event;
}

namespace native_viewport {

// A NativeViewportImpl is bound to a message pipe and to a PlatformViewport.
// The NativeViewportImpl's lifetime ends when either the message pipe is closed
// or the PlatformViewport informs the NativeViewportImpl that it has been
// destroyed.
class NativeViewportImpl : public mojo::NativeViewport,
                           public PlatformViewport::Delegate,
                           public mojo::ErrorHandler {
 public:
  NativeViewportImpl(bool is_headless,
                     const scoped_refptr<gles2::GpuState>& gpu_state,
                     mojo::InterfaceRequest<mojo::NativeViewport> request);
  ~NativeViewportImpl() override;

  // NativeViewport implementation.
  void Create(mojo::SizePtr size, const CreateCallback& callback) override;
  void RequestMetrics(const RequestMetricsCallback& callback) override;
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetSize(mojo::SizePtr size) override;
  void GetContextProvider(
      mojo::InterfaceRequest<mojo::ContextProvider> request) override;
  void SetEventDispatcher(
      mojo::NativeViewportEventDispatcherPtr dispatcher) override;

  // PlatformViewport::Delegate implementation.
  void OnMetricsChanged(mojo::ViewportMetricsPtr metrics) override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override;
  void OnAcceleratedWidgetDestroyed() override;
  bool OnEvent(mojo::EventPtr event) override;
  void OnDestroyed() override;

  // mojo::ErrorHandler implementation.
  void OnConnectionError() override;

 private:
  // Callback when the dispatcher has processed a message we're waiting on
  // an ack from. |pointer_id| identifies the pointer the message was associated
  // with.
  void AckEvent(int32 pointer_id);

  bool is_headless_;
  scoped_ptr<PlatformViewport> platform_viewport_;
  scoped_ptr<OnscreenContextProvider> context_provider_;
  bool sent_metrics_;
  mojo::ViewportMetricsPtr metrics_;
  CreateCallback create_callback_;
  RequestMetricsCallback metrics_callback_;
  mojo::NativeViewportEventDispatcherPtr event_dispatcher_;
  mojo::Binding<mojo::NativeViewport> binding_;

  // Set of pointer_ids we've sent a move to and are waiting on an ack.
  std::set<int32> pointers_waiting_on_ack_;

  base::WeakPtrFactory<NativeViewportImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportImpl);
};

}  // namespace native_viewport

#endif  // COMPONENTS_NATIVE_VIEWPORT_NATIVE_VIEWPORT_IMPL_H_
