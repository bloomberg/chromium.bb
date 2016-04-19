// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_

#include "base/synchronization/lock.h"
#include "cc/blink/context_provider_web_context.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/common/gpu/client/command_buffer_metrics.h"
#include "content/renderer/android/synchronous_compositor_factory.h"

namespace content {

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl();
  ~SynchronousCompositorFactoryImpl() override;

  // SynchronousCompositorFactory
  scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner()
      override;
  std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      int routing_id,
      uint32_t output_surface_id,
      const scoped_refptr<FrameSwapMessageQueue>& frame_swap_message_queue,
      const scoped_refptr<cc::ContextProvider>& onscreen_context,
      const scoped_refptr<cc::ContextProvider>& worker_context) override;
  InputHandlerManagerClient* GetInputHandlerManagerClient() override;
  SynchronousInputHandlerProxyClient* GetSynchronousInputHandlerProxyClient()
      override;
  std::unique_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) override;

  SynchronousInputEventFilter* synchronous_input_event_filter() {
    return &synchronous_input_event_filter_;
  }

 private:
  SynchronousInputEventFilter synchronous_input_event_filter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
