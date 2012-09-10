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
  if (!hung_plugin_showing_ || IsHung(base::TimeTicks::Now()))
    return;

  SendHungMessage(false);
  hung_plugin_showing_ = false;
}

bool PepperHungPluginFilter::IsHung(base::TimeTicks now) const {
  lock_.AssertAcquired();

  if (!pending_sync_message_count_)
    return false;  // Not blocked on a sync message.

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

  // Save this up front so all deltas computed below will agree on "now."
  base::TimeTicks now = base::TimeTicks::Now();

  if (!IsHung(now)) {
    if (pending_sync_message_count_ > 0) {
      // Got a timer message while we're waiting on a sync message. We need
      // to schedule another timer message because the latest sync message
      // would not have scheduled one (we only have one out-standing timer at
      // a time).
      DCHECK(!began_blocking_time_.is_null());
      base::TimeDelta time_blocked = now - began_blocking_time_;

      // This is the delay we're considered hung since we first started
      // blocking.
      base::TimeDelta delay =
          base::TimeDelta::FromSeconds(kHungThresholdSec) - time_blocked;

      // If another message was received from the plugin since we first started
      // blocking, but we haven't hit the "hard" threshold yet, we won't be
      // considered hung even though we're beyond the "regular" hung threshold.
      // In this case, schedule a timer at the hard threshold.
      if (delay <= base::TimeDelta()) {
        delay = base::TimeDelta::FromSeconds(kBlockedHardThresholdSec) -
            time_blocked;
      }

      // If the delay is negative or zero, another timer task will be
      // immediately posted, and if the condition persists we could end up
      // spinning on hang timers and not getting any work done, causing our
      // own "kind of" hang (although the message loop is running little work
      // will be getting done) in the renderer! If the timings or logic are
      // off, we'd prefer to get a crash dump and know about it.
      CHECK(delay <= base::TimeDelta());

      timer_task_pending_ = false;
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
