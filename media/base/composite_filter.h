// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_COMPOSITE_FILTER_H_
#define MEDIA_BASE_COMPOSITE_FILTER_H_

#include "base/threading/thread.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"

namespace media {

class CompositeFilter : public Filter {
 public:
  typedef base::Thread* (*ThreadFactoryFunction)(const char* thread_name);

  CompositeFilter(MessageLoop* message_loop);

  // Constructor that allows the default thread creation strategy to be
  // overridden.
  CompositeFilter(MessageLoop* message_loop,
                  ThreadFactoryFunction thread_factory);

  // Adds a filter to the composite. This is only allowed after set_host()
  // is called and before the first state changing operation such as Play(),
  // Flush(), Stop(), or Seek(). True is returned if the filter was successfully
  // added to the composite. False is returned if the filter couldn't be added
  // because the composite is in the wrong state or the filter needed a thread
  // and the composite was unable to create one.
  bool AddFilter(scoped_refptr<Filter> filter);

  // media::Filter methods.
  virtual const char* major_mime_type() const;
  virtual void set_host(FilterHost* host);
  virtual FilterHost* host();
  virtual bool requires_message_loop() const;
  virtual const char* message_loop_name() const;
  virtual void set_message_loop(MessageLoop* message_loop);
  virtual MessageLoop* message_loop();
  virtual void Play(FilterCallback* play_callback);
  virtual void Pause(FilterCallback* pause_callback);
  virtual void Flush(FilterCallback* flush_callback);
  virtual void Stop(FilterCallback* stop_callback);
  virtual void SetPlaybackRate(float playback_rate);
  virtual void Seek(base::TimeDelta time, FilterCallback* seek_callback);
  virtual void OnAudioRendererDisabled();

 protected:
  virtual ~CompositeFilter();

  /// Default thread factory strategy.
  static base::Thread* DefaultThreadFactory(const char* thread_name);

  void SetError(PipelineError error);

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

  // Initialization method called by constructors.
  void Init(MessageLoop* message_loop, ThreadFactoryFunction thread_factory);

  // Transition to a new state.
  void ChangeState(State new_state);

  // Start calling filters in a sequence.
  void StartSerialCallSequence();

  // Call filters in parallel.
  void StartParallelCallSequence();

  // Call the filter based on the current value of state_.
  void CallFilter(scoped_refptr<Filter>& filter, FilterCallback* callback);

  // Calls |callback_| and then clears the reference.
  void DispatchPendingCallback();

  // Gets the state to transition to given |state|.
  State GetNextState(State state) const;

  // Filter callback for a serial sequence.
  void SerialCallback();

  // Filter callback for a parallel sequence.
  void ParallelCallback();

  // Called when a parallel or serial call sequence completes.
  void OnCallSequenceDone();

  // Helper function for sending an error to the FilterHost.
  void SendErrorToHost(PipelineError error);

  // Helper function for handling errors during call sequences.
  void HandleError(PipelineError error);

  // Creates a callback that can be called from any thread, but is guaranteed
  // to call the specified method on the thread associated with this filter.
  FilterCallback* NewThreadSafeCallback(void (CompositeFilter::*method)());

  // Helper function used by NewThreadSafeCallback() to make sure the
  // method gets called on the right thread.
  static void OnCallback(MessageLoop* message_loop,
                         CancelableTask* task);

  // Helper function that indicates whether SetError() calls can be forwarded
  // to the host of this filter.
  bool CanForwardError();

  // Vector of threads owned by the composite and used by filters in |filters_|.
  typedef std::vector<base::Thread*> FilterThreadVector;
  FilterThreadVector filter_threads_;

  // Vector of the filters added to the composite.
  typedef std::vector<scoped_refptr<Filter> > FilterVector;
  FilterVector filters_;

  // Factory function used to create filter threads.
  ThreadFactoryFunction thread_factory_;

  // Callback for the pending request.
  scoped_ptr<FilterCallback> callback_;

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

  // Error passed in the last SetError() call.
  PipelineError error_;

  scoped_ptr<ScopedRunnableMethodFactory<CompositeFilter> > runnable_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositeFilter);
};

}  // namespace media

#endif  // MEDIA_BASE_COMPOSITE_FILTER_H_
