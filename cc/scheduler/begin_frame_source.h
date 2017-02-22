// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_BEGIN_FRAME_SOURCE_H_
#define CC_SCHEDULER_BEGIN_FRAME_SOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/small_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/delay_based_time_source.h"

namespace cc {

// (Pure) Interface for observing BeginFrame messages from BeginFrameSource
// objects.
class CC_EXPORT BeginFrameObserver {
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
  BeginFrameSource();
  virtual ~BeginFrameSource() {}

  // BeginFrameObservers use DidFinishFrame to acknowledge that they have
  // completed handling a BeginFrame.
  //
  // The DisplayScheduler uses these acknowledgments to trigger an early
  // deadline once all BeginFrameObservers have completed a frame.
  //
  // They also provide back pressure to a frame source about frame processing
  // (rather than toggling SetNeedsBeginFrames every frame). For example, the
  // BackToBackFrameSource uses them to make sure only one frame is pending at a
  // time.
  // TODO(eseckler): Use BeginFrameAcks in DisplayScheduler as described above.
  virtual void DidFinishFrame(BeginFrameObserver* obs,
                              const BeginFrameAck& ack) = 0;

  // Add/Remove an observer from the source. When no observers are added the BFS
  // should shut down its timers, disable vsync, etc.
  virtual void AddObserver(BeginFrameObserver* obs) = 0;
  virtual void RemoveObserver(BeginFrameObserver* obs) = 0;

  // Returns false if the begin frame source will just continue to produce
  // begin frames without waiting.
  virtual bool IsThrottled() const = 0;

  // Returns an identifier for this BeginFrameSource. Guaranteed unique within a
  // process, but not across processes. This is used to create BeginFrames that
  // originate at this source. Note that BeginFrameSources may pass on
  // BeginFrames created by other sources, with different IDs.
  uint32_t source_id() const;

 private:
  uint32_t source_id_;
};

// A BeginFrameSource that does nothing.
class CC_EXPORT StubBeginFrameSource : public BeginFrameSource {
 public:
  void DidFinishFrame(BeginFrameObserver* obs,
                      const BeginFrameAck& ack) override {}
  void AddObserver(BeginFrameObserver* obs) override {}
  void RemoveObserver(BeginFrameObserver* obs) override {}
  bool IsThrottled() const override;
};

// A frame source which ticks itself independently.
class CC_EXPORT SyntheticBeginFrameSource : public BeginFrameSource {
 public:
  ~SyntheticBeginFrameSource() override;

  virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) = 0;
  // This overrides any past or future interval from updating vsync parameters.
  virtual void SetAuthoritativeVSyncInterval(base::TimeDelta interval) = 0;
};

// A frame source which calls BeginFrame (at the next possible time) as soon as
// remaining frames reaches zero.
class CC_EXPORT BackToBackBeginFrameSource : public SyntheticBeginFrameSource,
                                             public DelayBasedTimeSourceClient {
 public:
  explicit BackToBackBeginFrameSource(
      std::unique_ptr<DelayBasedTimeSource> time_source);
  ~BackToBackBeginFrameSource() override;

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs,
                      const BeginFrameAck& ack) override;
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
class CC_EXPORT DelayBasedBeginFrameSource : public SyntheticBeginFrameSource,
                                             public DelayBasedTimeSourceClient {
 public:
  explicit DelayBasedBeginFrameSource(
      std::unique_ptr<DelayBasedTimeSource> time_source);
  ~DelayBasedBeginFrameSource() override;

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs,
                      const BeginFrameAck& ack) override {}
  bool IsThrottled() const override;

  // SyntheticBeginFrameSource implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;
  void SetAuthoritativeVSyncInterval(base::TimeDelta interval) override;

  // DelayBasedTimeSourceClient implementation.
  void OnTimerTick() override;

 private:
  BeginFrameArgs CreateBeginFrameArgs(base::TimeTicks frame_time,
                                      BeginFrameArgs::BeginFrameArgsType type);

  std::unique_ptr<DelayBasedTimeSource> time_source_;
  std::unordered_set<BeginFrameObserver*> observers_;
  base::TimeTicks last_timebase_;
  base::TimeDelta authoritative_interval_;
  BeginFrameArgs current_begin_frame_args_;
  uint64_t next_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(DelayBasedBeginFrameSource);
};

// Helper class that tracks outstanding acknowledgments from
// BeginFrameObservers.
class CC_EXPORT BeginFrameObserverAckTracker {
 public:
  BeginFrameObserverAckTracker();
  virtual ~BeginFrameObserverAckTracker();

  // The BeginFrameSource uses these methods to notify us when a BeginFrame was
  // started, an observer finished a frame, or an observer was added/removed.
  void OnBeginFrame(const BeginFrameArgs& args);
  void OnObserverFinishedFrame(BeginFrameObserver* obs,
                               const BeginFrameAck& ack);
  void OnObserverAdded(BeginFrameObserver* obs);
  void OnObserverRemoved(BeginFrameObserver* obs);

  // Returns |true| if all the source's observers completed the current frame.
  bool AllObserversFinishedFrame() const;

  // Returns |true| if any observer had damage during the current frame.
  bool AnyObserversHadDamage() const;

  // Return the sequence number of the latest frame that all active observers
  // have confirmed.
  uint64_t LatestConfirmedSequenceNumber() const;

 private:
  void SourceChanged(const BeginFrameArgs& args);

  uint32_t current_source_id_;
  uint64_t current_sequence_number_;
  // Small sets, but order matters for intersection computation.
  base::flat_set<BeginFrameObserver*> observers_;
  base::flat_set<BeginFrameObserver*> finished_observers_;
  bool observers_had_damage_;
  base::SmallMap<std::map<BeginFrameObserver*, uint64_t>, 4>
      latest_confirmed_sequence_numbers_;
};

class CC_EXPORT ExternalBeginFrameSourceClient {
 public:
  // Only called when changed.  Assumed false by default.
  virtual void OnNeedsBeginFrames(bool needs_begin_frames) = 0;
  // Called when all observers have completed a frame.
  virtual void OnDidFinishFrame(const BeginFrameAck& ack) = 0;
};

// A BeginFrameSource that is only ticked manually.  Usually the endpoint
// of messages from some other thread/process that send OnBeginFrame and
// receive SetNeedsBeginFrame messages.  This turns such messages back into
// an observable BeginFrameSource.
class CC_EXPORT ExternalBeginFrameSource : public BeginFrameSource {
 public:
  // Client lifetime must be preserved by owner past the lifetime of this class.
  explicit ExternalBeginFrameSource(ExternalBeginFrameSourceClient* client);
  ~ExternalBeginFrameSource() override;

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs,
                      const BeginFrameAck& ack) override;
  bool IsThrottled() const override;

  void OnSetBeginFrameSourcePaused(bool paused);
  void OnBeginFrame(const BeginFrameArgs& args);

 protected:
  void MaybeFinishFrame();
  void FinishFrame();

  BeginFrameArgs missed_begin_frame_args_;
  std::unordered_set<BeginFrameObserver*> observers_;
  ExternalBeginFrameSourceClient* client_;
  bool paused_ = false;
  bool frame_active_ = false;
  BeginFrameObserverAckTracker ack_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalBeginFrameSource);
};

}  // namespace cc

#endif  // CC_SCHEDULER_BEGIN_FRAME_SOURCE_H_
