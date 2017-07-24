// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_

#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "content/renderer/gpu/compositor_forwarding_message_filter.h"

namespace IPC {
class Message;
class SyncMessageFilter;
}

namespace content {

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread where cc::Scheduler is running.
//
// TODO(enne): This only implements the BeginFrameSource interface to
// make it easier to give to cc as an external begin frame source.  In the
// future, if this is owned by an output surface, then the internal
// viz::ExternalBeginFrameSource can be the BeginFrameSource passed to cc
// directly rather than proxied by this class.
class CompositorExternalBeginFrameSource
    : public viz::BeginFrameSource,
      public viz::ExternalBeginFrameSourceClient {
 public:
  explicit CompositorExternalBeginFrameSource(
      CompositorForwardingMessageFilter* filter,
      IPC::SyncMessageFilter* sync_message_filter,
      int routing_id);
  ~CompositorExternalBeginFrameSource() override;

  // viz::BeginFrameSource implementation.
  void AddObserver(viz::BeginFrameObserver* obs) override;
  void RemoveObserver(viz::BeginFrameObserver* obs) override;
  void DidFinishFrame(viz::BeginFrameObserver* obs) override;
  bool IsThrottled() const override;

  // viz::ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool need_begin_frames) override;

 private:
  class CompositorExternalBeginFrameSourceProxy
      : public base::RefCountedThreadSafe<
                   CompositorExternalBeginFrameSourceProxy> {
   public:
    explicit CompositorExternalBeginFrameSourceProxy(
        CompositorExternalBeginFrameSource* begin_frame_source)
        : begin_frame_source_(begin_frame_source) {}
    void ClearBeginFrameSource() { begin_frame_source_ = NULL; }
    void OnMessageReceived(const IPC::Message& message) {
      if (begin_frame_source_)
        begin_frame_source_->OnMessageReceived(message);
    }

   private:
    friend class base::RefCountedThreadSafe<
                     CompositorExternalBeginFrameSourceProxy>;
    virtual ~CompositorExternalBeginFrameSourceProxy() {}

    CompositorExternalBeginFrameSource* begin_frame_source_;

    DISALLOW_COPY_AND_ASSIGN(CompositorExternalBeginFrameSourceProxy);
  };

  void OnMessageReceived(const IPC::Message& message);
  void OnSetBeginFrameSourcePaused(bool paused);
  void OnBeginFrame(const viz::BeginFrameArgs& args);
  bool Send(IPC::Message* message);

  // Shared helper implementation.
  viz::ExternalBeginFrameSource external_begin_frame_source_;

  scoped_refptr<CompositorForwardingMessageFilter> begin_frame_source_filter_;
  scoped_refptr<CompositorExternalBeginFrameSourceProxy>
      begin_frame_source_proxy_;
  scoped_refptr<IPC::SyncMessageFilter> message_sender_;
  int routing_id_;
  CompositorForwardingMessageFilter::Handler begin_frame_source_filter_handler_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CompositorExternalBeginFrameSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
