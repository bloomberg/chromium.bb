// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_SYNC_CALL_BRIDGE_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_SYNC_CALL_BRIDGE_H_

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "content/common/input/sync_compositor_messages.h"
#include "content/public/browser/android/synchronous_compositor.h"

namespace content {

class SynchronousCompositorBrowserFilter;
class SynchronousCompositorHost;

// For the synchronous compositor feature of webview it is necessary
// that the UI thread to block until the renderer process has processed
// certain messages entirely. (beginframe and resulting compositor frames).
// This object is used to manage the waiting and signaling behavior on the UI
// thread. The UI thread will wait on a WaitableEvent (via FrameFuture class)
// or condition variable which is then signal by handlers in this class.
// This object is a cross thread object accessed both on the UI and IO threads.
//
// Examples of call graphs are:
//    Browser UI Thread         Browser IO Thread       Renderer
//
//  ->VSync Java
//      ----------------------------------------------->BeginFrame
//      CV Wait
//                                BeginFrameRes<----------
//                                CVSignal
//      WakeUp
//
//
//  ->DrawHwAsync
//      RegisterFrameFuture
//      ----------------------------------------------->DrawHwAsync
//      Do some stuff
//      FrameFuture::GetFrame()
//        WaitableEvent::Wait()
//                             ReceiveFrame<---------------
//                             WaitableEvent::Signal()
//      WakeUp
//
// This may seem simple but it gets a little more complicated when
// multiple views are involved. Each view will have it's own SyncCallBridge.
//
//   Once example is:
//
//    Browser UI Thread         Browser IO Thread       Renderer1    Renderer2
//
//  ->VSync Java
//      ----------------------------------------------->BeginFrame
//                                BeginFrameRes<----------
//                                CVSignal
//      ------------------------------------------------------------>BeginFrame
//      CV Wait
//                                BeginFrameRes<----------------------------
//                                CVSignal
//      WakeUp
//
// Notice that it is possible that before we wait on a CV variable a renderer
// may have already responded to the BeginFrame request.
//
class SynchronousCompositorSyncCallBridge
    : public base::RefCountedThreadSafe<SynchronousCompositorSyncCallBridge> {
 public:
  explicit SynchronousCompositorSyncCallBridge(SynchronousCompositorHost* host);

  // Attach a filter.
  void BindFilterOnUIThread();

  // Indicatation that the remote is now ready to process requests. Called
  // on either UI or IO thread.
  void RemoteReady();

  // Remote channel is closed signal all waiters.
  void RemoteClosedOnIOThread();

  // Receive a frame. Return false if the corresponding frame wasn't found.
  bool ReceiveFrameOnIOThread(int frame_sink_id,
                              uint32_t metadata_version,
                              base::Optional<viz::CompositorFrame>);

  // Receive a BeginFrameResponse. Returns true if handling the response was
  // successful or not.
  bool BeginFrameResponseOnIOThread(
      const SyncCompositorCommonRendererParams& render_params);

  // Schedule a callback for when vsync finishes and wait for the
  // BeginFrameResponse callback.
  bool WaitAfterVSyncOnUIThread(ui::WindowAndroid* window_android);

  // Store a FrameFuture for a later ReceiveFrame callback. Return if the
  // future was stored for further handling.
  bool SetFrameFutureOnUIThread(
      scoped_refptr<SynchronousCompositor::FrameFuture> frame_future);

  // Indicate the host is destroyed.
  void HostDestroyedOnUIThread();

  // Return whether the remote side is ready.
  bool IsRemoteReadyOnUIThread();

 private:
  friend class base::RefCountedThreadSafe<SynchronousCompositorSyncCallBridge>;
  ~SynchronousCompositorSyncCallBridge();

  // Callback passed to WindowAndroid, runs when the current vsync is completed.
  void VSyncCompleteOnUIThread();

  // Process metadata.
  void ProcessFrameMetadataOnUIThread(uint32_t metadata_version,
                                      viz::CompositorFrameMetadata metadata);

  void UnregisterSyncCallBridgeOnIOThread(
      scoped_refptr<SynchronousCompositorBrowserFilter> filter);

  // Signal all waiters for closure. Callee must host a lock to |lock_|.
  void SignalRemoteClosedToAllWaitersOnIOThread();

  // Post a task to unregister the bridge with the filter if necessary. Can
  // be called on either thread but must hold a lock to |lock_|.
  void UnregisterSyncCallBridgeIfNecessary();

  using FrameFutureQueue =
      base::circular_deque<scoped_refptr<SynchronousCompositor::FrameFuture>>;

  enum class RemoteState { INIT, READY, CLOSED };

  const int routing_id_;

  // UI thread only.
  ui::WindowAndroid* window_android_in_vsync_ = nullptr;
  SynchronousCompositorHost* host_;
  bool bound_to_filter_ = false;
  bool mojo_enabled_;

  // Shared variables between the IO thread and UI thread.
  base::Lock lock_;
  FrameFutureQueue frame_futures_;
  bool begin_frame_response_valid_ = false;
  SyncCompositorCommonRendererParams last_render_params_;
  base::ConditionVariable begin_frame_condition_;
  RemoteState remote_state_ = RemoteState::INIT;

  // IO thread based callback that will unbind this object from
  // the SynchronousCompositorBrowserFilter. Only called once
  // all pending frames are acknowledged.
  base::OnceClosure unregister_callback_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorSyncCallBridge);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_SYNC_CALL_BRIDGE_H_
