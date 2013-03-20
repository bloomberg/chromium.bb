// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/base/cc_export.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

class Thread;

struct ScheduledActionDrawAndSwapResult {
  ScheduledActionDrawAndSwapResult()
      : did_draw(false),
        did_swap(false) {}
  ScheduledActionDrawAndSwapResult(bool did_draw, bool did_swap)
      : did_draw(did_draw),
        did_swap(did_swap) {}
  bool did_draw;
  bool did_swap;
};

class SchedulerClient {
 public:
  virtual void ScheduledActionBeginFrame() = 0;
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapIfPossible() = 0;
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapForced() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionCheckForCompletedTileUploads() = 0;
  virtual void ScheduledActionActivatePendingTreeIfNeeded() = 0;
  virtual void ScheduledActionBeginContextRecreation() = 0;
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() = 0;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class CC_EXPORT Scheduler : FrameRateControllerClient {
 public:
  static scoped_ptr<Scheduler> Create(
      SchedulerClient* client,
      scoped_ptr<FrameRateController> frame_rate_controller,
      const SchedulerSettings& scheduler_settings) {
    return make_scoped_ptr(new Scheduler(
        client, frame_rate_controller.Pass(), scheduler_settings));
  }

  virtual ~Scheduler();

  void SetCanBeginFrame(bool can);

  void SetVisible(bool visible);
  void SetCanDraw(bool can_draw);
  void SetHasPendingTree(bool has_pending_tree);

  void SetNeedsCommit();

  // Like SetNeedsCommit(), but ensures a commit will definitely happen even if
  // we are not visible.
  void SetNeedsForcedCommit();

  void SetNeedsRedraw();

  void SetMainThreadNeedsLayerTextures();

  // Like SetNeedsRedraw(), but ensures the draw will definitely happen even if
  // we are not visible.
  void SetNeedsForcedRedraw();

  void DidSwapUseIncompleteTile();

  void BeginFrameComplete();
  void BeginFrameAborted();

  void SetMaxFramesPending(int max);
  int MaxFramesPending() const;

  void SetSwapBuffersCompleteSupported(bool supported);
  void DidSwapBuffersComplete();

  void DidLoseOutputSurface();
  void DidRecreateOutputSurface();

  bool CommitPending() const { return state_machine_.CommitPending(); }
  bool RedrawPending() const { return state_machine_.RedrawPending(); }

  void SetTimebaseAndInterval(base::TimeTicks timebase,
                              base::TimeDelta interval);

  base::TimeTicks AnticipatedDrawTime();

  base::TimeTicks LastVSyncTime();

  // FrameRateControllerClient implementation
  virtual void VSyncTick(bool throttled) OVERRIDE;

 private:
  Scheduler(SchedulerClient* client,
            scoped_ptr<FrameRateController> frame_rate_controller,
            const SchedulerSettings& scheduler_settings);

  void ProcessScheduledActions();

  const SchedulerSettings settings_;
  SchedulerClient* client_;
  scoped_ptr<FrameRateController> frame_rate_controller_;
  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
