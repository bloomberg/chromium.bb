// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_COMPOSITE_FILTER_H_
#define MEDIA_BASE_COMPOSITE_FILTER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"

class MessageLoop;

namespace media {

class MEDIA_EXPORT CompositeFilter : public Filter {
 public:
  explicit CompositeFilter(MessageLoop* message_loop);

  // Adds a filter to the composite. This is only allowed after set_host()
  // is called and before the first state changing operation such as Play(),
  // Flush(), Stop(), or Seek(). True is returned if the filter was successfully
  // added to the composite. False is returned if the filter couldn't be added
  // because the composite is in the wrong state.
  bool AddFilter(scoped_refptr<Filter> filter);

  // media::Filter methods.
  virtual void set_host(FilterHost* host) OVERRIDE;
  virtual FilterHost* host() OVERRIDE;
  virtual void Play(const base::Closure& play_callback) OVERRIDE;
  virtual void Pause(const base::Closure& pause_callback) OVERRIDE;
  virtual void Flush(const base::Closure& flush_callback) OVERRIDE;
  virtual void Stop(const base::Closure& stop_callback) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual void Seek(
      base::TimeDelta time, const FilterStatusCB& seek_cb) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;

 protected:
  virtual ~CompositeFilter();

  void SetError(PipelineStatus error);

 private:
  class FilterHostImpl;

  enum State {
    kInvalid,
    kCreated,
    kPaused,
    kPlayPending,
    kStopWhilePlayPending,
    kPlaying,
    kPausePending,
    kStopWhilePausePending,
    kFlushPending,
    kStopWhileFlushPending,
    kSeekPending,
    kStopWhileSeekPending,
    kStopPending,
    kStopped,
    kError
  };

  // Transition to a new state.
  void ChangeState(State new_state);

  // Start calling filters in a sequence.
  void StartSerialCallSequence();

  // Call filters in parallel.
  void StartParallelCallSequence();

  // Call the filter based on the current value of state_.
  void CallFilter(scoped_refptr<Filter>& filter, const base::Closure& callback);

  // Calls |callback_| and then clears the reference.
  void DispatchPendingCallback(PipelineStatus status);

  // Gets the state to transition to given |state|.
  State GetNextState(State state) const;

  // Filter callback for a serial sequence.
  void SerialCallback();

  // Filter callback for a parallel sequence.
  void ParallelCallback();

  // Called when a parallel or serial call sequence completes.
  void OnCallSequenceDone();

  // Helper function for sending an error to the FilterHost.
  void SendErrorToHost(PipelineStatus error);

  // Creates a callback that can be called from any thread, but is guaranteed
  // to call the specified method on the thread associated with this filter.
  base::Closure NewThreadSafeCallback(void (CompositeFilter::*method)());

  // Helper function used by NewThreadSafeCallback() to make sure the
  // method gets called on the right thread.
  static void OnCallback(MessageLoop* message_loop,
                         const base::Closure& closure);

  // Helper function that indicates whether SetError() calls can be forwarded
  // to the host of this filter.
  bool CanForwardError();

  // Checks to see if a Play(), Pause(), Flush(), Stop(), or Seek() operation is
  // pending.
  bool IsOperationPending() const;

  // Called by operations that take a FilterStatusCB instead of a Closure.
  // TODO: Remove when Closures are replaced by FilterStatusCB.
  void OnStatusCB(const base::Closure& callback, PipelineStatus status);

  // Vector of the filters added to the composite.
  typedef std::vector<scoped_refptr<Filter> > FilterVector;
  FilterVector filters_;

  // Callback for the pending request.
  // TODO: Remove callback_ when Closures are replaced by FilterStatusCB.
  base::Closure callback_;
  FilterStatusCB status_cb_;

  // Time parameter for the pending Seek() request.
  base::TimeDelta pending_seek_time_;

  // Current state of this filter.
  State state_;

  // The index of the filter currently processing a request.
  unsigned int sequence_index_;

  // Message loop passed into the constructor.
  MessageLoop* message_loop_;

  // FilterHost implementation passed to Filters owned by this
  // object.
  scoped_ptr<FilterHostImpl> host_impl_;

  // PIPELINE_OK, or last error passed to SetError().
  PipelineStatus status_;

  base::WeakPtrFactory<CompositeFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositeFilter);
};

}  // namespace media

#endif  // MEDIA_BASE_COMPOSITE_FILTER_H_
