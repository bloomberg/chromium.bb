// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_message_filter.h"

#include "base/logging.h"
#include "base/process.h"
#include "base/process_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/result_codes.h"

BrowserMessageFilter::BrowserMessageFilter()
    : channel_(NULL), peer_handle_(base::kNullProcessHandle) {
}

BrowserMessageFilter::~BrowserMessageFilter() {
  base::CloseProcessHandle(peer_handle_);
}

void BrowserMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void BrowserMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

void BrowserMessageFilter::OnChannelConnected(int32 peer_pid) {
  if (!base::OpenProcessHandle(peer_pid, &peer_handle_)) {
    NOTREACHED();
  }
}

bool BrowserMessageFilter::Send(IPC::Message* message) {
  if (message->is_sync()) {
    // We don't support sending synchronous messages from the browser.  If we
    // really needed it, we can make this class derive from SyncMessageFilter
    // but it seems better to not allow sending synchronous messages from the
    // browser, since it might allow a corrupt/malicious renderer to hang us.
    NOTREACHED() << "Can't send sync message through BrowserMessageFilter!";
    return false;
  }

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(this, &BrowserMessageFilter::Send, message));
    return true;
  }

  if (channel_)
    return channel_->Send(message);

  delete message;
  return false;
}

void BrowserMessageFilter::OverrideThreadForMessage(const IPC::Message& message,
                                                    BrowserThread::ID* thread) {
}

bool BrowserMessageFilter::OnMessageReceived(const IPC::Message& message) {
  BrowserThread::ID thread = BrowserThread::IO;
  OverrideThreadForMessage(message, &thread);
  if (thread == BrowserThread::IO)
    return DispatchMessage(message);

  BrowserThread::PostTask(
      thread, FROM_HERE,
      NewRunnableMethod(
          this, &BrowserMessageFilter::DispatchMessage, message));
  return true;
}

bool BrowserMessageFilter::DispatchMessage(const IPC::Message& message) {
  bool message_was_ok = true;
  bool rv = OnMessageReceived(message, &message_was_ok);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO) || rv) <<
      "Must handle messages that were dispatched to another thread!";
  if (!message_was_ok) {
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_BMF"));
    BadMessageReceived();
  }

  return rv;
}

void BrowserMessageFilter::BadMessageReceived() {
  base::KillProcess(peer_handle(), ResultCodes::KILLED_BAD_MESSAGE, false);
}
