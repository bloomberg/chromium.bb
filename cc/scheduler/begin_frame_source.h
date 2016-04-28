// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_BEGIN_FRAME_SOURCE_H_
#define CC_SCHEDULER_BEGIN_FRAME_SOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/delay_based_time_source.h"

#ifdef NDEBUG
#define DEBUG_FRAMES(...)
#else
#define DEBUG_FRAMES(name, arg1_name, arg1_val, arg2_name, arg2_val)         \
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"), name, \
               arg1_name, arg1_val, arg2_name, arg2_val);
#endif

namespace cc {

// (Pure) Interface for observing BeginFrame messages from BeginFrameSource
// objects.
class CC_EXPORT BeginFrameObserver {
 public:
  virtual ~BeginFrameObserver() {}

  // The |args| given to OnBeginFrame is guaranteed to have
  // |args|.IsValid()==true and have |args|.frame_time
  // field be strictly greater than the previous call.
  //
  // Side effects: This function can (and most of the time *will*) change the
  // return value of the LastUsedBeginFrameArgs method. See the documentation
  // on that method for more information.
  virtual void OnBeginFrame(const BeginFrameArgs& args) = 0;

  // Returns the last BeginFrameArgs used by the observer. This method's return
  // value is affected by the OnBeginFrame method!
  //
  //  - Before the first call of OnBeginFrame, this method should return a
  //    BeginFrameArgs on which IsValid() returns false.
  //
  //  - If the |args| passed to OnBeginFrame is (or *will be*) used, then
  //    LastUsedBeginFrameArgs return value should become the |args| given to
  //    OnBeginFrame.
  //
  //  - If the |args| passed to OnBeginFrame is dropped, then
  //    LastUsedBeginFrameArgs return value should *not* change.
  //
  // These requirements are designed to allow chaining and nesting of
  // BeginFrameObservers which filter the incoming BeginFrame messages while
  // preventing "double dropping" and other bad side effects.
  virtual const BeginFrameArgs& LastUsedBeginFrameArgs() const = 0;

  virtual void OnBeginFrameSourcePausedChanged(bool paused) = 0;
};

// Simple base class which implements a BeginFrameObserver which checks the
// incoming values meet the BeginFrameObserver requirements and implements the
// required LastUsedBeginFrameArgs behaviour.
//
// Users of this class should;
//  - Implement the OnBeginFrameDerivedImpl function.
//  - Recommended (but not required) to call
//    BeginFrameObserverBase::OnValueInto in their overridden OnValueInto
//    function.
class CC_EXPORT BeginFrameObserverBase : public BeginFrameObserver {
 public:
  BeginFrameObserverBase();

  // BeginFrameObserver

  // Traces |args| and DCHECK |args| satisfies pre-conditions then calls
  // OnBeginFrameDerivedImpl and updates the last_begin_frame_args_ value on
  // true.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;

 protected:
  // Subclasses should override this method!
  // Return true if the given argument is (or will be) used.
  virtual bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) = 0;

  BeginFrameArgs last_begin_frame_args_;
  int64_t dropped_begin_frame_args_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BeginFrameObserverBase);
};

// Interface for a class which produces BeginFrame calls to a
// BeginFrameObserver.
//
// BeginFrame calls *normally* occur just after a vsync interrupt when input
// processing has been finished and provide information about the time values
// of the vsync times. *However*, these values can be heavily modified or even
// plain made up (when no vsync signal is available or vsync throttling is
// turned off). See the BeginFrameObserver for information about the guarantees
// all BeginFrameSources *must* provide.
class CC_EXPORT BeginFrameSource {
 public:
  virtual ~BeginFrameSource() {}

  // DidFinishFrame provides back pressure to a frame source about frame
  // processing (rather than toggling SetNeedsBeginFrames every frame). It is
  // used by systems like the BackToBackFrameSource to make sure only one frame
  // is pending at a time.
  virtual void DidFinishFrame(BeginFrameObserver* obs,
                              size_t remaining_frames) = 0;

  // Add/Remove an observer from the source. When no observers are added the BFS
  // should shut down its timers, disable vsync, etc.
  virtual void AddObserver(BeginFrameObserver* obs) = 0;
  virtual void RemoveObserver(BeginFrameObserver* obs) = 0;
};

// Simple base class which implements a BeginFrameSource.
// Implementation classes should:
//  - Implement the pure virtual (Set)NeedsBeginFrames methods from
//    BeginFrameSource.
//  - Use the CallOnBeginFrame method to call to the observer(s).
class CC_EXPORT BeginFrameSourceBase : public BeginFrameSource {
 public:
  ~BeginFrameSourceBase() override;

  // BeginFrameSource
  void DidFinishFrame(BeginFrameObserver* obs,
                      size_t remaining_frames) override {}
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;

 protected:
  BeginFrameSourceBase();

  // These methods should be used by subclasses to make the call to the
  // observers.
  void CallOnBeginFrame(const BeginFrameArgs& args);
  void SetBeginFrameSourcePaused(bool paused);

  // This notifies that the subclass that it must turn on or off its mechnanism
  // for producing BeginFrames.
  virtual void OnNeedsBeginFramesChanged(bool needs_begin_frames) {}

  bool needs_begin_frames() const { return !observers_.empty(); }

  std::set<BeginFrameObserver*> observers_;
  bool paused_;

 private:
  bool inside_as_value_into_;

  DISALLOW_COPY_AND_ASSIGN(BeginFrameSourceBase);
};

// A frame source which calls BeginFrame (at the next possible time) as soon as
// remaining frames reaches zero.
class CC_EXPORT BackToBackBeginFrameSource : public BeginFrameSourceBase {
 public:
  explicit BackToBackBeginFrameSource(
      base::SingleThreadTaskRunner* task_runner);
  ~BackToBackBeginFrameSource() override;

  // BeginFrameSource
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs,
                      size_t remaining_frames) override;

 protected:
  virtual base::TimeTicks Now();  // Now overridable for testing

  base::SingleThreadTaskRunner* task_runner_;
  base::CancelableClosure begin_frame_task_;
  std::set<BeginFrameObserver*> pending_begin_frame_observers_;

  void PostPendingBeginFramesTask();
  void SendPendingBeginFrames();

 private:
  base::WeakPtrFactory<BackToBackBeginFrameSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackToBackBeginFrameSource);
};

// A frame source which is locked to an external parameters provides from a
// vsync source and generates BeginFrameArgs for it.
class CC_EXPORT SyntheticBeginFrameSource : public BeginFrameSourceBase,
                                            public DelayBasedTimeSourceClient {
 public:
  explicit SyntheticBeginFrameSource(base::SingleThreadTaskRunner* task_runner,
                                     base::TimeDelta initial_vsync_interval);
  explicit SyntheticBeginFrameSource(
      std::unique_ptr<DelayBasedTimeSource> time_source);
  ~SyntheticBeginFrameSource() override;

  void OnUpdateVSyncParameters(base::TimeTicks new_vsync_timebase,
                               base::TimeDelta new_vsync_interval);

  // BeginFrameSourceBase
  void AddObserver(BeginFrameObserver* obs) override;
  void OnNeedsBeginFramesChanged(bool needs_begin_frames) override;

  // DelayBasedTimeSourceClient
  void OnTimerTick() override;

 protected:
  BeginFrameArgs CreateBeginFrameArgs(base::TimeTicks frame_time,
                                      BeginFrameArgs::BeginFrameArgsType type);

  std::unique_ptr<DelayBasedTimeSource> time_source_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyntheticBeginFrameSource);
};

}  // namespace cc

#endif  // CC_SCHEDULER_BEGIN_FRAME_SOURCE_H_
