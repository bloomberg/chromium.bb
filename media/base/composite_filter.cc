// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/composite_filter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
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
  virtual void SetNaturalVideoSize(const gfx::Size& size) OVERRIDE;
  virtual void NotifyEnded() OVERRIDE;
  virtual void DisableAudioRenderer() OVERRIDE;

 private:
  CompositeFilter* parent_;
  FilterHost* host_;

  DISALLOW_COPY_AND_ASSIGN(FilterHostImpl);
};

CompositeFilter::CompositeFilter(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : state_(kCreated),
      sequence_index_(0),
      message_loop_(message_loop),
      status_(PIPELINE_OK),
      weak_ptr_factory_(this) {
  DCHECK(message_loop);
}

CompositeFilter::~CompositeFilter() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kCreated || state_ == kStopped);

  filters_.clear();
}

void CompositeFilter::AddFilter(scoped_refptr<Filter> filter) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  CHECK(filter && state_ == kCreated && host());

  // Register ourselves as the filter's host.
  filter->set_host(host_impl_.get());
  filters_.push_back(make_scoped_refptr(filter.get()));
}

void CompositeFilter::RemoveFilter(scoped_refptr<Filter> filter) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!filter.get() || state_ != kCreated || !host())
    LOG(FATAL) << "Unknown filter, or in unexpected state.";

  for (FilterVector::iterator it = filters_.begin();
       it != filters_.end(); ++it) {
    if (it->get() != filter.get())
      continue;
    filters_.erase(it);
    filter->clear_host();
    return;
  }
  LOG(FATAL) << "Filter missing.";
}

void CompositeFilter::set_host(FilterHost* host) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(host);
  DCHECK(!host_impl_.get());
  host_impl_.reset(new FilterHostImpl(this, host));
}

FilterHost* CompositeFilter::host() {
  return host_impl_.get() ? host_impl_->host() : NULL;
}

void CompositeFilter::Play(const base::Closure& play_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (IsOperationPending()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    play_cb.Run();
    return;
  } else if (state_ == kPlaying) {
    play_cb.Run();
    return;
  } else if (!host() || (state_ != kPaused && state_ != kCreated)) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    play_cb.Run();
    return;
  }

  ChangeState(kPlayPending);
  callback_ = play_cb;
  StartSerialCallSequence();
}

void CompositeFilter::Pause(const base::Closure& pause_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (IsOperationPending()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    pause_cb.Run();
    return;
  } else if (state_ == kPaused) {
    pause_cb.Run();
    return;
  } else if (!host() || state_ != kPlaying) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    pause_cb.Run();
    return;
  }

  ChangeState(kPausePending);
  callback_ = pause_cb;
  StartSerialCallSequence();
}

void CompositeFilter::Flush(const base::Closure& flush_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (IsOperationPending()) {
    SendErrorToHost(PIPELINE_ERROR_OPERATION_PENDING);
    flush_cb.Run();
    return;
  } else if (!host() || (state_ != kCreated && state_ != kPaused)) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    flush_cb.Run();
    return;
  }

  ChangeState(kFlushPending);
  callback_ = flush_cb;
  StartParallelCallSequence();
}

void CompositeFilter::Stop(const base::Closure& stop_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!host()) {
    SendErrorToHost(PIPELINE_ERROR_INVALID_STATE);
    stop_cb.Run();
    return;
  } else if (state_ == kStopped) {
    stop_cb.Run();
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
      stop_cb.Run();
      return;
  }

  if (!status_cb_.is_null()) {
    DCHECK_EQ(state_, kStopWhileSeekPending);
    status_cb_.Reset();
  }

  callback_ = stop_cb;
  if (state_ == kStopPending) {
    StartSerialCallSequence();
  }
}

void CompositeFilter::SetPlaybackRate(float playback_rate) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  for (FilterVector::iterator iter = filters_.begin();
       iter != filters_.end();
       ++iter) {
    (*iter)->SetPlaybackRate(playback_rate);
  }
}

void CompositeFilter::Seek(base::TimeDelta time,
                           const PipelineStatusCB& seek_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());

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
  DCHECK(message_loop_->BelongsToCurrentThread());
  for (FilterVector::iterator iter = filters_.begin();
       iter != filters_.end();
       ++iter) {
    (*iter)->OnAudioRendererDisabled();
  }
}

void CompositeFilter::ChangeState(State new_state) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  state_ = new_state;
}

void CompositeFilter::StartSerialCallSequence() {
  DCHECK(message_loop_->BelongsToCurrentThread());
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
  DCHECK(message_loop_->BelongsToCurrentThread());
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
    base::ResetAndReturn(&status_cb_).Run(status);
    return;
  }

  if (!callback_.is_null()) {
    if (status != PIPELINE_OK)
      SendErrorToHost(status);
    base::ResetAndReturn(&callback_).Run();
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
  DCHECK(message_loop_->BelongsToCurrentThread());
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
  DCHECK(message_loop_->BelongsToCurrentThread());

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
static void TrampolineClosureIfNecessary(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const base::Closure& closure) {
  if (message_loop->BelongsToCurrentThread())
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
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&CompositeFilter::SetError, this, error));
    return;
  }

  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(state_, kCreated);

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

void CompositeFilter::FilterHostImpl::SetNaturalVideoSize(
    const gfx::Size& size) {
  host_->SetNaturalVideoSize(size);
}

void CompositeFilter::FilterHostImpl::NotifyEnded() {
  host_->NotifyEnded();
}

void CompositeFilter::FilterHostImpl::DisableAudioRenderer() {
  host_->DisableAudioRenderer();
}

}  // namespace media
