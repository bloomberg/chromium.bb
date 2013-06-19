// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"
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
  virtual void SetNeedsBeginFrameOnImplThread(bool enable) = 0;
  virtual void ScheduledActionSendBeginFrameToMainThread() = 0;
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapIfPossible() = 0;
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapForced() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionCheckForCompletedTileUploads() = 0;
  virtual void ScheduledActionActivatePendingTreeIfNeeded() = 0;
  virtual void ScheduledActionBeginOutputSurfaceCreation() = 0;
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() = 0;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) = 0;
  virtual base::TimeDelta DrawDurationEstimate() = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class CC_EXPORT Scheduler {
 public:
  static scoped_ptr<Scheduler> Create(
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings) {
    return make_scoped_ptr(new Scheduler(client,  scheduler_settings));
  }

  virtual ~Scheduler();

  void SetCanStart();

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

  void FinishCommit();
  void BeginFrameAbortedByMainThread();

  void DidLoseOutputSurface();
  void DidCreateAndInitializeOutputSurface();
  bool HasInitializedOutputSurface() const {
    return state_machine_.HasInitializedOutputSurface();
  }

  bool CommitPending() const { return state_machine_.CommitPending(); }
  bool RedrawPending() const { return state_machine_.RedrawPending(); }

  bool WillDrawIfNeeded() const;

  base::TimeTicks AnticipatedDrawTime();

  base::TimeTicks LastBeginFrameOnImplThreadTime();

  void BeginFrame(const BeginFrameArgs& args);

  std::string StateAsStringForTesting() { return state_machine_.ToString(); }

 private:
  Scheduler(SchedulerClient* client,
            const SchedulerSettings& scheduler_settings);

  void SetupNextBeginFrameIfNeeded();
  void DrawAndSwapIfPossible();
  void DrawAndSwapForced();
  void ProcessScheduledActions();

  const SchedulerSettings settings_;
  SchedulerClient* client_;

  base::WeakPtrFactory<Scheduler> weak_factory_;
  bool last_set_needs_begin_frame_;
  bool has_pending_begin_frame_;
  // TODO(brianderson): crbug.com/249806 : Remove safe_to_expect_begin_frame_
  // workaround.
  bool safe_to_expect_begin_frame_;
  BeginFrameArgs last_begin_frame_args_;

  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
