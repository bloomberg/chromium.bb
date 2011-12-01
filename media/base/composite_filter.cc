// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/composite_filter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/stl_util.h"

namespace media {

class CompositeFilter::FilterHostImpl : public FilterHost {
 public:
  FilterHostImpl(CompositeFilter* parent, FilterHost* host);

  FilterHost* host();

  // media::FilterHost methods.
  virtual void SetError(PipelineStatus error) OVERRIDE;
  virtual base::TimeDelta GetTime() const OVERRIDE;
  virtual base::TimeDelta GetDuration() const OVERRIDE;
  virtual void SetTime(base::TimeDelta time) OVERRIDE;
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE;
  virtual void SetBufferedTime(base::TimeDelta buffered_time) OVERRIDE;
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual void SetBufferedBytes(int64 buffered_bytes) OVERRIDE;
  virtual void SetNaturalVideoSize(const gfx::Size& size) OVERRIDE;
  virtual void NotifyEnded() OVERRIDE;
  virtual void SetNetworkActivity(bool network_activity) OVERRIDE;
  virtual void DisableAudioRenderer() OVERRIDE;
  virtual void SetCurrentReadPosition(int64 offset) OVERRIDE;
  virtual int64 GetCurrentReadPosition() OVERRIDE;

 private:
  CompositeFilter* parent_;
  FilterHost* host_;

  DISALLOW_COPY_AND_ASSIGN(FilterHostImpl);
};

CompositeFilter::CompositeFilter(MessageLoop* message_loop)
    : state_(kCreated),
      sequence_index_(0),
      message_loop_(message_loop),
      status_(PIPELINE_OK),
      weak_ptr_factory_(this) {
  DCHECK(message_loop);
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

void CompositeFilter::Play(const base::Closure& play_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (IsOperationPending()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    play_callback.Run();
    return;
  } else if (state_ == kPlaying) {
    play_callback.Run();
    return;
  } else if (!host() || (state_ != kPaused && state_ != kCreated)) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    play_callback.Run();
    return;
  }

  ChangeState(kPlayPending);
  callback_ = play_callback;
  StartSerialCallSequence();
}

void CompositeFilter::Pause(const base::Closure& pause_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (IsOperationPending()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    pause_callback.Run();
    return;
  } else if (state_ == kPaused) {
    pause_callback.Run();
    return;
  } else if (!host() || state_ != kPlaying) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    pause_callback.Run();
    return;
  }

  ChangeState(kPausePending);
  callback_ = pause_callback;
  StartSerialCallSequence();
}

void CompositeFilter::Flush(const base::Closure& flush_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (IsOperationPending()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    flush_callback.Run();
    return;
  } else if (!host() || (state_ != kCreated && state_ != kPaused)) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    flush_callback.Run();
    return;
  }

  ChangeState(kFlushPending);
  callback_ = flush_callback;
  StartParallelCallSequence();
}

void CompositeFilter::Stop(const base::Closure& stop_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (!host()) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    stop_callback.Run();
    return;
  } else if (state_ == kStopped) {
    stop_callback.Run();
    return;
  }

  switch (state_) {
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
      stop_callback.Run();
      return;
  }

  if (!status_cb_.is_null()) {
    DCHECK_EQ(state_, kStopWhileSeekPending);
    status_cb_.Reset();
  }

  callback_ = stop_callback;
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
                           const FilterStatusCB& seek_cb) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (IsOperationPending()) {
    seek_cb.Run(PIPELINE_ERROR_OPERATION_PENDING);
    return;
  } else if (!host() || (state_ != kPaused && state_ != kCreated)) {
    seek_cb.Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  ChangeState(kSeekPending);
  status_cb_ = seek_cb;
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
  sequence_index_ = 0;

  if (!filters_.empty()) {
    CallFilter(filters_[sequence_index_],
               NewThreadSafeCallback(&CompositeFilter::SerialCallback));
  } else {
    SerialCallback();
  }
}

void CompositeFilter::StartParallelCallSequence() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  status_ = PIPELINE_OK;
  sequence_index_ = 0;

  if (!filters_.empty()) {
    for (size_t i = 0; i < filters_.size(); i++) {
      CallFilter(filters_[i],
                 NewThreadSafeCallback(&CompositeFilter::ParallelCallback));
    }
  } else {
    ParallelCallback();
  }
}

void CompositeFilter::CallFilter(scoped_refptr<Filter>& filter,
                                 const base::Closure& callback) {
  switch (state_) {
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
      filter->Seek(pending_seek_time_,
                   base::Bind(&CompositeFilter::OnStatusCB, this, callback));
      break;
    default:
      ChangeState(kError);
      DispatchPendingCallback(PIPELINE_ERROR_INVALID_STATE);
  }
}

void CompositeFilter::DispatchPendingCallback(PipelineStatus status) {
  DCHECK(status_cb_.is_null() ^ callback_.is_null());

  if (!status_cb_.is_null()) {
    ResetAndRunCB(&status_cb_, status);
    return;
  }

  if (!callback_.is_null()) {
    if (status != PIPELINE_OK)
      SendErrorToHost(status);
    ResetAndRunCB(&callback_);
  }
}

CompositeFilter::State CompositeFilter::GetNextState(State state) const {
  State ret = kInvalid;
  switch (state) {
    case kPlayPending:
      ret = kPlaying;
      break;
    case kPausePending:
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
    DispatchPendingCallback(status_);
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
      DispatchPendingCallback(status_);
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
    DispatchPendingCallback(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  ChangeState(next_state);

  if (state_ == kStopPending) {
    // Handle a deferred Stop().
    StartSerialCallSequence();
  } else {
    // Call the callback to indicate that the operation has completed.
    DispatchPendingCallback(PIPELINE_OK);
  }
}

void CompositeFilter::SendErrorToHost(PipelineStatus error) {
  if (host_impl_.get())
    host_impl_.get()->host()->SetError(error);
}

// Execute |closure| if on |message_loop|, otherwise post to it.
static void TrampolineClosureIfNecessary(MessageLoop* message_loop,
                                         const base::Closure& closure) {
  if (MessageLoop::current() == message_loop)
    closure.Run();
  else
    message_loop->PostTask(FROM_HERE, closure);
}

base::Closure CompositeFilter::NewThreadSafeCallback(
    void (CompositeFilter::*method)()) {
  return base::Bind(&TrampolineClosureIfNecessary,
                    message_loop_,
                    base::Bind(method, weak_ptr_factory_.GetWeakPtr()));
}

bool CompositeFilter::CanForwardError() {
  return (state_ == kCreated) || (state_ == kPlaying) || (state_ == kPaused);
}

bool CompositeFilter::IsOperationPending() const {
  DCHECK(callback_.is_null() || status_cb_.is_null());

  return !callback_.is_null() || !status_cb_.is_null();
}

void CompositeFilter::OnStatusCB(const base::Closure& callback,
                                 PipelineStatus status) {
  if (status != PIPELINE_OK)
    SetError(status);

  callback.Run();
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
        base::Bind(&CompositeFilter::SetError, this, error));
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

void CompositeFilter::FilterHostImpl::SetNaturalVideoSize(
    const gfx::Size& size) {
  host_->SetNaturalVideoSize(size);
}

void CompositeFilter::FilterHostImpl::NotifyEnded() {
  host_->NotifyEnded();
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
