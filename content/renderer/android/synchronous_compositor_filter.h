// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_FILTER_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_FILTER_H_

#include <stdint.h>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/android/synchronous_compositor_registry.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "ipc/ipc_sender.h"
#include "ipc/message_filter.h"

namespace ui {
class SynchronousInputHandlerProxy;
}

namespace content {

class SynchronousCompositorProxy;

class SynchronousCompositorFilter : public IPC::MessageFilter,
                                    public IPC::Sender,
                                    public SynchronousCompositorRegistry,
                                    public InputHandlerManagerClient {
 public:
  SynchronousCompositorFilter(const scoped_refptr<base::SingleThreadTaskRunner>&
                                  compositor_task_runner);

  // IPC::MessageFilter overrides.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override;

  // IPC::Sender overrides.
  bool Send(IPC::Message* message) override;

  // SynchronousCompositorRegistry overrides.
  void RegisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface) override;
  void UnregisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface) override;
  void RegisterBeginFrameSource(int routing_id,
                                SynchronousCompositorExternalBeginFrameSource*
                                    begin_frame_source) override;
  void UnregisterBeginFrameSource(int routing_id,
                                  SynchronousCompositorExternalBeginFrameSource*
                                      begin_frame_source) override;

  // InputHandlerManagerClient overrides.
  void SetBoundHandler(const Handler& handler) override;
  void DidAddInputHandler(
      int routing_id,
      ui::SynchronousInputHandlerProxy*
          synchronous_input_handler_proxy) override;
  void DidRemoveInputHandler(int routing_id) override;
  void DidOverscroll(int routing_id,
                     const DidOverscrollParams& params) override;
  void DidStopFlinging(int routing_id) override;

 private:
  ~SynchronousCompositorFilter() override;

  // IO thread methods.
  void SendOnIOThread(IPC::Message* message);

  // Compositor thread methods.
  void FilterReadyyOnCompositorThread();
  void OnMessageReceivedOnCompositorThread(const IPC::Message& message);
  void SetBoundHandlerOnCompositorThread(const Handler& handler);
  void CheckIsReady(int routing_id);
  void UnregisterObjects(int routing_id);
  void RemoveEntryIfNeeded(int routing_id);
  SynchronousCompositorProxy* FindProxy(int routing_id);

  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // The sender_ only gets invoked on the thread corresponding to io_loop_.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  IPC::Sender* sender_;

  // Compositor thread-only fields.
  using SyncCompositorMap =
      base::ScopedPtrHashMap<int /* routing_id */,
                             scoped_ptr<SynchronousCompositorProxy>>;
  SyncCompositorMap sync_compositor_map_;
  Handler input_handler_;

  bool filter_ready_;
  struct Entry {
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source;
    SynchronousCompositorOutputSurface* output_surface;
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy;

    Entry();
    bool IsReady();
  };
  using EntryMap = base::hash_map<int, Entry>;
  EntryMap entry_map_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_FILTER_H_
