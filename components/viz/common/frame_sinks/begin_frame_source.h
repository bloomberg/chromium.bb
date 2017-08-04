// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_BEGIN_FRAME_SOURCE_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_BEGIN_FRAME_SOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"

namespace viz {

// (Pure) Interface for observing BeginFrame messages from BeginFrameSource
// objects.
class VIZ_COMMON_EXPORT BeginFrameObserver {
 public:
  virtual ~BeginFrameObserver() {}

  // The |args| given to OnBeginFrame is guaranteed to have
  // |args|.IsValid()==true. If |args|.source_id did not change between
  // invocations, |args|.sequence_number is guaranteed to be be strictly greater
  // than the previous call. Further, |args|.frame_time is guaranteed to be
  // greater than or equal to the previous call even if the source_id changes.
  //
  // Side effects: This function can (and most of the time *will*) change the
  // return value of the LastUsedBeginFrameArgs method. See the documentation
  // on that method for more information.
  //
  // The observer is required call BeginFrameSource::DidFinishFrame() as soon as
  // it has completed handling the BeginFrame.
  virtual void OnBeginFrame(const BeginFrameArgs& args) = 0;

  // Returns the last BeginFrameArgs used by the observer. This method's
  // return value is affected by the OnBeginFrame method!
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
class VIZ_COMMON_EXPORT BeginFrameObserverBase : public BeginFrameObserver {
 public:
  BeginFrameObserverBase();
  ~BeginFrameObserverBase() override;

  // BeginFrameObserver

  // Traces |args| and DCHECK |args| satisfies pre-conditions then calls
  // OnBeginFrameDerivedImpl and updates the last_begin_frame_args_ value on
  // true.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;

 protected:
  // Return true if the given argument is (or will be) used.
  virtual bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) = 0;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  BeginFrameArgs last_begin_frame_args_;
  int64_t dropped_begin_frame_args_ = 0;

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
class VIZ_COMMON_EXPORT BeginFrameSource {
 public:
  BeginFrameSource();
  virtual ~BeginFrameSource();

  // Returns an identifier for this BeginFrameSource. Guaranteed unique within a
  // process, but not across processes. This is used to create BeginFrames that
  // originate at this source. Note that BeginFrameSources may pass on
  // BeginFrames created by other sources, with different IDs.
  uint32_t source_id() const { return source_id_; }

  // BeginFrameObservers use DidFinishFrame to provide back pressure to a frame
  // source about frame processing (rather than toggling SetNeedsBeginFrames
  // every frame). For example, the BackToBackFrameSource uses them to make sure
  // only one frame is pending at a time.
  virtual void DidFinishFrame(BeginFrameObserver* obs) = 0;

  // Add/Remove an observer from the source. When no observers are added the BFS
  // should shut down its timers, disable vsync, etc.
  virtual void AddObserver(BeginFrameObserver* obs) = 0;
  virtual void RemoveObserver(BeginFrameObserver* obs) = 0;

  // Returns false if the begin frame source will just continue to produce
  // begin frames without waiting.
  virtual bool IsThrottled() const = 0;

  virtual void AsValueInto(base::trace_event::TracedValue* state) const;

 private:
  uint32_t source_id_;

  DISALLOW_COPY_AND_ASSIGN(BeginFrameSource);
};

// A BeginFrameSource that does nothing.
class VIZ_COMMON_EXPORT StubBeginFrameSource : public BeginFrameSource {
 public:
  void DidFinishFrame(BeginFrameObserver* obs) override {}
  void AddObserver(BeginFrameObserver* obs) override {}
  void RemoveObserver(BeginFrameObserver* obs) override {}
  bool IsThrottled() const override;
};

// A frame source which ticks itself independently.
class VIZ_COMMON_EXPORT SyntheticBeginFrameSource : public BeginFrameSource {
 public:
  ~SyntheticBeginFrameSource() override;

  virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) = 0;
  // This overrides any past or future interval from updating vsync parameters.
  virtual void SetAuthoritativeVSyncInterval(base::TimeDelta interval) = 0;
};

// A frame source which calls BeginFrame (at the next possible time) as soon as
// an observer acknowledges the prior BeginFrame.
class VIZ_COMMON_EXPORT BackToBackBeginFrameSource
    : public SyntheticBeginFrameSource,
      public DelayBasedTimeSourceClient {
 public:
  explicit BackToBackBeginFrameSource(
      std::unique_ptr<DelayBasedTimeSource> time_source);
  ~BackToBackBeginFrameSource() override;

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs) override;
  bool IsThrottled() const override;

  // SyntheticBeginFrameSource implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override {}
  void SetAuthoritativeVSyncInterval(base::TimeDelta interval) override {}

  // DelayBasedTimeSourceClient implementation.
  void OnTimerTick() override;

 private:
  std::unique_ptr<DelayBasedTimeSource> time_source_;
  std::unordered_set<BeginFrameObserver*> observers_;
  std::unordered_set<BeginFrameObserver*> pending_begin_frame_observers_;
  uint64_t next_sequence_number_;
  base::WeakPtrFactory<BackToBackBeginFrameSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackToBackBeginFrameSource);
};

// A frame source which is locked to an external parameters provides from a
// vsync source and generates BeginFrameArgs for it.
class VIZ_COMMON_EXPORT DelayBasedBeginFrameSource
    : public SyntheticBeginFrameSource,
      public DelayBasedTimeSourceClient {
 public:
  explicit DelayBasedBeginFrameSource(
      std::unique_ptr<DelayBasedTimeSource> time_source);
  ~DelayBasedBeginFrameSource() override;

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs) override {}
  bool IsThrottled() const override;

  // SyntheticBeginFrameSource implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;
  void SetAuthoritativeVSyncInterval(base::TimeDelta interval) override;

  // DelayBasedTimeSourceClient implementation.
  void OnTimerTick() override;

 private:
  BeginFrameArgs CreateBeginFrameArgs(base::TimeTicks frame_time);

  std::unique_ptr<DelayBasedTimeSource> time_source_;
  std::unordered_set<BeginFrameObserver*> observers_;
  base::TimeTicks last_timebase_;
  base::TimeDelta authoritative_interval_;
  BeginFrameArgs last_begin_frame_args_;
  uint64_t next_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(DelayBasedBeginFrameSource);
};

class VIZ_COMMON_EXPORT ExternalBeginFrameSourceClient {
 public:
  // Only called when changed.  Assumed false by default.
  virtual void OnNeedsBeginFrames(bool needs_begin_frames) = 0;
};

// A BeginFrameSource that is only ticked manually.  Usually the endpoint
// of messages from some other thread/process that send OnBeginFrame and
// receive SetNeedsBeginFrame messages.  This turns such messages back into
// an observable BeginFrameSource.
class VIZ_COMMON_EXPORT ExternalBeginFrameSource : public BeginFrameSource {
 public:
  // Client lifetime must be preserved by owner past the lifetime of this class.
  explicit ExternalBeginFrameSource(ExternalBeginFrameSourceClient* client);
  ~ExternalBeginFrameSource() override;

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs) override {}
  bool IsThrottled() const override;
  void AsValueInto(base::trace_event::TracedValue* state) const override;

  void OnSetBeginFrameSourcePaused(bool paused);
  void OnBeginFrame(const BeginFrameArgs& args);

 protected:
  // Called on AddObserver and gets missed BeginFrameArgs for the given
  // observer. The missed BeginFrame is sent only if the returned
  // BeginFrameArgs is valid.
  virtual BeginFrameArgs GetMissedBeginFrameArgs(BeginFrameObserver* obs);

  BeginFrameArgs last_begin_frame_args_;
  std::unordered_set<BeginFrameObserver*> observers_;
  ExternalBeginFrameSourceClient* client_;
  bool paused_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalBeginFrameSource);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_BEGIN_FRAME_SOURCE_H_
