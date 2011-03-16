// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/composite_filter.h"

#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "media/base/callback.h"

namespace media {

class CompositeFilter::FilterHostImpl : public FilterHost {
 public:
  FilterHostImpl(CompositeFilter* parent, FilterHost* host);

  FilterHost* host();

  // media::FilterHost methods.
  virtual void SetError(PipelineStatus error);
  virtual base::TimeDelta GetTime() const;
  virtual base::TimeDelta GetDuration() const;
  virtual void SetTime(base::TimeDelta time);
  virtual void SetDuration(base::TimeDelta duration);
  virtual void SetBufferedTime(base::TimeDelta buffered_time);
  virtual void SetTotalBytes(int64 total_bytes);
  virtual void SetBufferedBytes(int64 buffered_bytes);
  virtual void SetVideoSize(size_t width, size_t height);
  virtual void SetStreaming(bool streaming);
  virtual void NotifyEnded();
  virtual void SetLoaded(bool loaded);
  virtual void SetNetworkActivity(bool network_activity);
  virtual void DisableAudioRenderer();
  virtual void SetCurrentReadPosition(int64 offset);
  virtual int64 GetCurrentReadPosition();

 private:
  CompositeFilter* parent_;
  FilterHost* host_;

  DISALLOW_COPY_AND_ASSIGN(FilterHostImpl);
};

CompositeFilter::CompositeFilter(MessageLoop* message_loop)
    : state_(kCreated),
      sequence_index_(0),
      message_loop_(message_loop),
      status_(PIPELINE_OK) {
  DCHECK(message_loop);
  runnable_factory_.reset(
      new ScopedRunnableMethodFactory<CompositeFilter>(this));
}

CompositeFilter::~CompositeFilter() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(state_ == kCreated || state_ == kStopped);

  filters_.clear();
}

bool CompositeFilter::AddFilter(scoped_refptr<Filter> filter) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (!filter.get() || state_ != kCreated || !host())
    return false;

  // Register ourselves as the filter's host.
  filter->set_host(host_impl_.get());
  filters_.push_back(make_scoped_refptr(filter.get()));
  return true;
}

void CompositeFilter::set_host(FilterHost* host) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(host);
  DCHECK(!host_impl_.get());
  host_impl_.reset(new FilterHostImpl(this, host));
}

FilterHost* CompositeFilter::host() {
  return host_impl_.get() ? host_impl_->host() : NULL;
}

void CompositeFilter::Play(FilterCallback* play_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  scoped_ptr<FilterCallback> callback(play_callback);
  if (callback_.get()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    callback->Run();
    return;
  } else if (state_ == kPlaying) {
    callback->Run();
    return;
  } else if (!host() || (state_ != kPaused && state_ != kCreated)) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    callback->Run();
    return;
  }

  ChangeState(kPlayPending);
  callback_.reset(callback.release());
  StartSerialCallSequence();
}

void CompositeFilter::Pause(FilterCallback* pause_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  scoped_ptr<FilterCallback> callback(pause_callback);
  if (callback_.get()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    callback->Run();
    return;
  } else if (state_ == kPaused) {
    callback->Run();
    return;
  } else if (!host() || state_ != kPlaying) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    callback->Run();
    return;
  }

  ChangeState(kPausePending);
  callback_.reset(callback.release());
  StartSerialCallSequence();
}

void CompositeFilter::Flush(FilterCallback* flush_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  scoped_ptr<FilterCallback> callback(flush_callback);
  if (callback_.get()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    callback->Run();
    return;
  } else if (!host() || (state_ != kCreated && state_ != kPaused)) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    callback->Run();
    return;
  }

  ChangeState(kFlushPending);
  callback_.reset(callback.release());
  StartParallelCallSequence();
}

void CompositeFilter::Stop(FilterCallback* stop_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  scoped_ptr<FilterCallback> callback(stop_callback);
  if (!host()) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    callback->Run();
    return;
  } else if (state_ == kStopped) {
    callback->Run();
    return;
  }

  switch(state_) {
    case kError:
    case kCreated:
    case kPaused:
    case kPlaying:
      ChangeState(kStopPending);
      break;
    case kPlayPending:
      ChangeState(kStopWhilePlayPending);
      break;
    case kPausePending:
      ChangeState(kStopWhilePausePending);
      break;
    case kFlushPending:
      ChangeState(kStopWhileFlushPending);
      break;
    case kSeekPending:
      ChangeState(kStopWhileSeekPending);
      break;
    default:
      SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
      callback->Run();
      return;
  }

  callback_.reset(callback.release());
  if (state_ == kStopPending) {
    StartSerialCallSequence();
  }
}

void CompositeFilter::SetPlaybackRate(float playback_rate) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  for (FilterVector::iterator iter = filters_.begin();
       iter != filters_.end();
       ++iter) {
    (*iter)->SetPlaybackRate(playback_rate);
  }
}

void CompositeFilter::Seek(base::TimeDelta time,
                           FilterCallback* seek_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  scoped_ptr<FilterCallback> callback(seek_callback);
  if (callback_.get()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    callback->Run();
    return;
  } else if (!host() || (state_ != kPaused && state_ != kCreated)) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    callback->Run();
    return;
  }

  ChangeState(kSeekPending);
  callback_.reset(callback.release());
  pending_seek_time_ = time;
  StartSerialCallSequence();
}

void CompositeFilter::OnAudioRendererDisabled() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  for (FilterVector::iterator iter = filters_.begin();
       iter != filters_.end();
       ++iter) {
    (*iter)->OnAudioRendererDisabled();
  }
}

void CompositeFilter::ChangeState(State new_state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  state_ = new_state;
}

void CompositeFilter::StartSerialCallSequence() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  status_ = PIPELINE_OK;

  if (!filters_.empty()) {
    sequence_index_ = 0;
    CallFilter(filters_[sequence_index_],
               NewThreadSafeCallback(&CompositeFilter::SerialCallback));
  } else {
    sequence_index_ = 0;
    SerialCallback();
  }
}

void CompositeFilter::StartParallelCallSequence() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  status_ = PIPELINE_OK;

  if (!filters_.empty()) {
    sequence_index_ = 0;
    for (size_t i = 0; i < filters_.size(); i++) {
      CallFilter(filters_[i],
                 NewThreadSafeCallback(&CompositeFilter::ParallelCallback));
    }
  } else {
    sequence_index_ = 0;
    ParallelCallback();
  }
}

void CompositeFilter::CallFilter(scoped_refptr<Filter>& filter,
                                 FilterCallback* callback) {
  switch(state_) {
    case kPlayPending:
      filter->Play(callback);
      break;
    case kPausePending:
      filter->Pause(callback);
      break;
    case kFlushPending:
      filter->Flush(callback);
      break;
    case kStopPending:
      filter->Stop(callback);
      break;
    case kSeekPending:
      filter->Seek(pending_seek_time_, callback);
      break;
    default:
      delete callback;
      ChangeState(kError);
      HandleError(PIPELINE_ERROR_INVALID_STATE);
  }
}

void CompositeFilter::DispatchPendingCallback() {
  if (callback_.get()) {
    scoped_ptr<FilterCallback> callback(callback_.release());
    callback->Run();
  }
}

CompositeFilter::State CompositeFilter::GetNextState(State state) const {
  State ret = kInvalid;
  switch (state) {
    case kPlayPending:
      ret = kPlaying;
      break;
    case kPausePending:
      ret = kPaused;
    case kFlushPending:
      ret = kPaused;
      break;
    case kStopPending:
      ret = kStopped;
      break;
    case kSeekPending:
      ret = kPaused;
      break;
    case kStopWhilePlayPending:
    case kStopWhilePausePending:
    case kStopWhileFlushPending:
    case kStopWhileSeekPending:
      ret = kStopPending;
      break;

    case kInvalid:
    case kCreated:
    case kPlaying:
    case kPaused:
    case kStopped:
    case kError:
      ret = kInvalid;
      break;

    // default: intentionally left out to catch missing states.
  }

  return ret;
}

void CompositeFilter::SerialCallback() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (status_ != PIPELINE_OK) {
    // We encountered an error. Terminate the sequence now.
    ChangeState(kError);
    HandleError(status_);
    return;
  }

  if (!filters_.empty())
    sequence_index_++;

  if (sequence_index_ == filters_.size()) {
    // All filters have been successfully called without error.
    OnCallSequenceDone();
  } else if (GetNextState(state_) == kStopPending) {
    // Abort sequence early and start issuing Stop() calls.
    ChangeState(kStopPending);
    StartSerialCallSequence();
  } else {
    // We aren't done with the sequence. Call the next filter.
    CallFilter(filters_[sequence_index_],
               NewThreadSafeCallback(&CompositeFilter::SerialCallback));
  }
}

void CompositeFilter::ParallelCallback() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!filters_.empty())
    sequence_index_++;

  if (sequence_index_ == filters_.size()) {
    if (status_ != PIPELINE_OK) {
      // We encountered an error.
      ChangeState(kError);
      HandleError(status_);
      return;
    }

    OnCallSequenceDone();
  }
}

void CompositeFilter::OnCallSequenceDone() {
  State next_state = GetNextState(state_);

  if (next_state == kInvalid) {
    // We somehow got into an unexpected state.
    ChangeState(kError);
    HandleError(PIPELINE_ERROR_INVALID_STATE);
  }

  ChangeState(next_state);

  if (state_ == kStopPending) {
    // Handle a deferred Stop().
    StartSerialCallSequence();
  } else {
    // Call the callback to indicate that the operation has completed.
    DispatchPendingCallback();
  }
}

void CompositeFilter::SendErrorToHost(PipelineStatus error) {
  if (host_impl_.get())
    host_impl_.get()->host()->SetError(error);
}

void CompositeFilter::HandleError(PipelineStatus error) {
  DCHECK_NE(error, PIPELINE_OK);
  SendErrorToHost(error);
  DispatchPendingCallback();
}

FilterCallback* CompositeFilter::NewThreadSafeCallback(
    void (CompositeFilter::*method)()) {
  return TaskToCallbackAdapter::NewCallback(
      NewRunnableFunction(&CompositeFilter::OnCallback,
                          message_loop_,
                          runnable_factory_->NewRunnableMethod(method)));
}

// This method is intentionally static so that no reference to the composite
// is needed to call it. This method may be called by other threads and we
// don't want those threads to gain ownership of this composite by having a
// reference to it. |task| will contain a weak reference to the composite
// so that the reference can be cleared if the composite is destroyed before
// the callback is called.
// static
void CompositeFilter::OnCallback(MessageLoop* message_loop,
                                 CancelableTask* task) {
  if (MessageLoop::current() != message_loop) {
    // Posting callback to the proper thread.
    message_loop->PostTask(FROM_HERE, task);
    return;
  }

  task->Run();
  delete task;
}

bool CompositeFilter::CanForwardError() {
  return (state_ == kCreated) || (state_ == kPlaying) || (state_ == kPaused);
}

void CompositeFilter::SetError(PipelineStatus error) {
  // TODO(acolwell): Temporary hack to handle errors that occur
  // during filter initialization. In this case we just forward
  // the error to the host even if it is on the wrong thread. We
  // have to do this because if we defer the call, we can't be
  // sure the host will get the error before the "init done" callback
  // is executed. This will be cleaned up when filter init is refactored.
  if (state_ == kCreated) {
    SendErrorToHost(error);
    return;
  }

  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &CompositeFilter::SetError, error));
    return;
  }

  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Drop errors recieved while stopping or stopped.
  // This shields the owner of this object from having
  // to deal with errors it can't do anything about.
  if (state_ == kStopPending || state_ == kStopped)
    return;

  status_ = error;
  if (CanForwardError())
    SendErrorToHost(error);
}

CompositeFilter::FilterHostImpl::FilterHostImpl(CompositeFilter* parent,
                                                FilterHost* host)
    : parent_(parent),
      host_(host) {
}

FilterHost* CompositeFilter::FilterHostImpl::host() {
  return host_;
}

// media::FilterHost methods.
void CompositeFilter::FilterHostImpl::SetError(PipelineStatus error) {
  parent_->SetError(error);
}

base::TimeDelta CompositeFilter::FilterHostImpl::GetTime() const {
  return host_->GetTime();
}

base::TimeDelta CompositeFilter::FilterHostImpl::GetDuration() const {
  return host_->GetDuration();
}

void CompositeFilter::FilterHostImpl::SetTime(base::TimeDelta time) {
  host_->SetTime(time);
}

void CompositeFilter::FilterHostImpl::SetDuration(base::TimeDelta duration) {
  host_->SetDuration(duration);
}

void CompositeFilter::FilterHostImpl::SetBufferedTime(
    base::TimeDelta buffered_time) {
  host_->SetBufferedTime(buffered_time);
}

void CompositeFilter::FilterHostImpl::SetTotalBytes(int64 total_bytes) {
  host_->SetTotalBytes(total_bytes);
}

void CompositeFilter::FilterHostImpl::SetBufferedBytes(int64 buffered_bytes) {
  host_->SetBufferedBytes(buffered_bytes);
}

void CompositeFilter::FilterHostImpl::SetVideoSize(size_t width,
                                                   size_t height) {
  host_->SetVideoSize(width, height);
}

void CompositeFilter::FilterHostImpl::SetStreaming(bool streaming) {
  host_->SetStreaming(streaming);
}

void CompositeFilter::FilterHostImpl::NotifyEnded() {
  host_->NotifyEnded();
}

void CompositeFilter::FilterHostImpl::SetLoaded(bool loaded) {
  host_->SetLoaded(loaded);
}

void CompositeFilter::FilterHostImpl::SetNetworkActivity(
    bool network_activity) {
  host_->SetNetworkActivity(network_activity);
}

void CompositeFilter::FilterHostImpl::DisableAudioRenderer() {
  host_->DisableAudioRenderer();
}

void CompositeFilter::FilterHostImpl::SetCurrentReadPosition(int64 offset) {
  host_->SetCurrentReadPosition(offset);
}

int64 CompositeFilter::FilterHostImpl::GetCurrentReadPosition() {
  return host_->GetCurrentReadPosition();
}

}  // namespace media
