// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_REMOTE_CHANNEL_MAIN_H_
#define CC_TREES_REMOTE_CHANNEL_MAIN_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/trees/channel_main.h"
#include "cc/trees/remote_proto_channel.h"
#include "cc/trees/task_runner_provider.h"

namespace cc {
class ProxyMain;

namespace proto {
class CompositorMessage;
}

class CC_EXPORT RemoteChannelMain : public ChannelMain,
                                    public RemoteProtoChannel::ProtoReceiver {
 public:
  static scoped_ptr<RemoteChannelMain> Create(
      RemoteProtoChannel* remote_proto_channel,
      ProxyMain* proxy_main,
      TaskRunnerProvider* task_runner_provider);

  ~RemoteChannelMain() override;

  // ChannelMain implementation
  void SetThrottleFrameProductionOnImpl(bool throttle) override;
  void UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                    TopControlsState current,
                                    bool animate) override;
  void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) override;
  void MainThreadHasStoppedFlingingOnImpl() override;
  void SetInputThrottledUntilCommitOnImpl(bool is_throttled) override;
  void SetDeferCommitsOnImpl(bool defer_commits) override;
  void FinishAllRenderingOnImpl(CompletionEvent* completion) override;
  void SetVisibleOnImpl(bool visible) override;
  void ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) override;
  void MainFrameWillHappenOnImplForTesting(
      CompletionEvent* completion,
      bool* main_frame_will_happen) override;
  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) override;
  void SetNeedsCommitOnImpl() override;
  void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time) override;
  void StartCommitOnImpl(CompletionEvent* completion,
                         LayerTreeHost* layer_tree_host,
                         base::TimeTicks main_thread_start_time,
                         bool hold_commit_for_activation) override;
  void SynchronouslyInitializeImpl(
      LayerTreeHost* layer_tree_host,
      scoped_ptr<BeginFrameSource> external_begin_frame_source) override;
  void SynchronouslyCloseImpl() override;

  // RemoteProtoChannel::ProtoReceiver implementation
  void OnProtoReceived(scoped_ptr<proto::CompositorMessage> proto) override;

 protected:
  RemoteChannelMain(RemoteProtoChannel* remote_proto_channel,
                    ProxyMain* proxy_main,
                    TaskRunnerProvider* task_runner_provider);

 private:
  void SendMessageProto(const proto::CompositorMessage& proto);

  RemoteProtoChannel* remote_proto_channel_;
  ProxyMain* proxy_main_;
  TaskRunnerProvider* task_runner_provider_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(RemoteChannelMain);
};

}  // namespace cc

#endif  // CC_TREES_REMOTE_CHANNEL_MAIN_H_
