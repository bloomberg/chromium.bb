// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/scheduler/begin_frame_source.h"
#include "content/renderer/gpu/compositor_forwarding_message_filter.h"

namespace IPC {
class Message;
class SyncMessageFilter;
}

namespace content {

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread where cc::Scheduler is running.
class CompositorExternalBeginFrameSource
    : public cc::BeginFrameSourceBase,
      public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  explicit CompositorExternalBeginFrameSource(
      CompositorForwardingMessageFilter* filter,
      IPC::SyncMessageFilter* sync_message_filter,
      int routing_id);
  ~CompositorExternalBeginFrameSource() override;

  // cc::BeginFrameSourceBase implementation.
  void AddObserver(cc::BeginFrameObserver* obs) override;
  void OnNeedsBeginFramesChanged(bool needs_begin_frames) override;
  void SetClientReady() override;

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

  void OnBeginFrame(const cc::BeginFrameArgs& args);
  bool Send(IPC::Message* message);

  scoped_refptr<CompositorForwardingMessageFilter> begin_frame_source_filter_;
  scoped_refptr<CompositorExternalBeginFrameSourceProxy>
      begin_frame_source_proxy_;
  scoped_refptr<IPC::SyncMessageFilter> message_sender_;
  int routing_id_;
  CompositorForwardingMessageFilter::Handler begin_frame_source_filter_handler_;
  cc::BeginFrameArgs missed_begin_frame_args_;

  DISALLOW_COPY_AND_ASSIGN(CompositorExternalBeginFrameSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_EXTERNAL_BEGIN_FRAME_SOURCE_H_
