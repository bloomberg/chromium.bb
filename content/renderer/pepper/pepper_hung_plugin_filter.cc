// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_hung_plugin_filter.h"

#include "base/bind.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

namespace {

// We'll consider the plugin hung after not hearing anything for this long.
const int kHungThresholdSec = 10;

// If we ever are blocked for this long, we'll consider the plugin hung, even
// if we continue to get messages (which is why the above hung threshold never
// kicked in). Maybe the plugin is spamming us with events and never unblocking
// and never processing our sync message.
const int kBlockedHardThresholdSec = kHungThresholdSec * 1.5;

}  // namespace

PepperHungPluginFilter::PepperHungPluginFilter(const FilePath& plugin_path,
                                               int view_routing_id,
                                               int plugin_child_id)
    : plugin_path_(plugin_path),
      view_routing_id_(view_routing_id),
      plugin_child_id_(plugin_child_id),
      filter_(RenderThread::Get()->GetSyncMessageFilter()),
      io_loop_(RenderThreadImpl::current()->GetIOLoopProxy()),
      pending_sync_message_count_(0),
      hung_plugin_showing_(false),
      timer_task_pending_(false) {
}

void PepperHungPluginFilter::BeginBlockOnSyncMessage() {
  base::AutoLock lock(lock_);
  if (pending_sync_message_count_ == 0)
    began_blocking_time_ = base::TimeTicks::Now();
  pending_sync_message_count_++;

  EnsureTimerScheduled();
}

void PepperHungPluginFilter::EndBlockOnSyncMessage() {
  base::AutoLock lock(lock_);
  pending_sync_message_count_--;
  DCHECK(pending_sync_message_count_ >= 0);

  MayHaveBecomeUnhung();
}

void PepperHungPluginFilter::OnFilterAdded(IPC::Channel* channel) {
}

void PepperHungPluginFilter::OnFilterRemoved() {
  base::AutoLock lock(lock_);
  MayHaveBecomeUnhung();
}

void PepperHungPluginFilter::OnChannelError() {
  base::AutoLock lock(lock_);
  MayHaveBecomeUnhung();
}

bool PepperHungPluginFilter::OnMessageReceived(const IPC::Message& message) {
  // Just track incoming message times but don't handle any messages.
  base::AutoLock lock(lock_);
  last_message_received_ = base::TimeTicks::Now();
  MayHaveBecomeUnhung();
  return false;
}

PepperHungPluginFilter::~PepperHungPluginFilter() {}

void PepperHungPluginFilter::EnsureTimerScheduled() {
  lock_.AssertAcquired();
  if (timer_task_pending_)
    return;

  timer_task_pending_ = true;
  io_loop_->PostDelayedTask(FROM_HERE,
      base::Bind(&PepperHungPluginFilter::OnHangTimer, this),
      base::TimeDelta::FromSeconds(kHungThresholdSec));
}

void PepperHungPluginFilter::MayHaveBecomeUnhung() {
  if (!hung_plugin_showing_ || IsHung())
    return;

  SendHungMessage(false);
  hung_plugin_showing_ = false;
}

bool PepperHungPluginFilter::IsHung() const {
  lock_.AssertAcquired();

  if (!pending_sync_message_count_)
    return false;  // Not blocked on a sync message.

  base::TimeTicks now = base::TimeTicks::Now();

  DCHECK(!began_blocking_time_.is_null());
  if (now - began_blocking_time_ >=
      base::TimeDelta::FromSeconds(kBlockedHardThresholdSec))
    return true;  // Been blocked too long regardless of what the plugin is
                  // sending us.

  base::TimeDelta hung_threshold =
      base::TimeDelta::FromSeconds(kHungThresholdSec);
  if (now - began_blocking_time_ >= hung_threshold &&
      (last_message_received_.is_null() ||
       now - last_message_received_ >= hung_threshold))
    return true;  // Blocked and haven't received a message in too long.

  return false;
}

void PepperHungPluginFilter::OnHangTimer() {
  base::AutoLock lock(lock_);
  timer_task_pending_ = false;

  if (!IsHung()) {
    if (pending_sync_message_count_ > 0) {
      // Got a timer message while we're waiting on a sync message. We need
      // to schedule another timer message because the latest sync message
      // would not have scheduled one (we only have one out-standing timer at
      // a time).
      DCHECK(!began_blocking_time_.is_null());
      base::TimeDelta delay =
          base::TimeDelta::FromSeconds(kHungThresholdSec) -
          (base::TimeTicks::Now() - began_blocking_time_);
      io_loop_->PostDelayedTask(FROM_HERE,
          base::Bind(&PepperHungPluginFilter::OnHangTimer, this),
          delay);
    }
    return;
  }

  hung_plugin_showing_ = true;
  SendHungMessage(true);
}

void PepperHungPluginFilter::SendHungMessage(bool is_hung) {
  filter_->Send(new ViewHostMsg_PepperPluginHung(
      view_routing_id_, plugin_child_id_, plugin_path_, is_hung));
}

}  // namespace content
