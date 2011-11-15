// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/process_util.h"
#include "content/browser/user_metrics.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_sync_message.h"

using content::BrowserThread;

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
        base::IgnoreReturn<bool>(
            base::Bind(&BrowserMessageFilter::Send, this, message)));
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

  if (thread == BrowserThread::UI && !CheckCanDispatchOnUI(message, this))
    return true;

  BrowserThread::PostTask(
      thread, FROM_HERE,
      base::IgnoreReturn<bool>(
          base::Bind(&BrowserMessageFilter::DispatchMessage, this, message)));
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
  base::KillProcess(peer_handle(), content::RESULT_CODE_KILLED_BAD_MESSAGE,
                    false);
}

bool BrowserMessageFilter::CheckCanDispatchOnUI(const IPC::Message& message,
                                                IPC::Message::Sender* sender) {
#if defined(OS_WIN) && !defined(USE_AURA)
  // On Windows there's a potential deadlock with sync messsages going in
  // a circle from browser -> plugin -> renderer -> browser.
  // On Linux we can avoid this by avoiding sync messages from browser->plugin.
  // On Mac we avoid this by not supporting windowed plugins.
  if (message.is_sync() && !message.is_caller_pumping_messages()) {
    // NOTE: IF YOU HIT THIS ASSERT, THE SOLUTION IS ALMOST NEVER TO RUN A
    // NESTED MESSAGE LOOP IN THE RENDERER!!!
    // That introduces reentrancy which causes hard to track bugs.  You should
    // find a way to either turn this into an asynchronous message, or one
    // that can be answered on the IO thread.
    NOTREACHED() << "Can't send sync messages to UI thread without pumping "
        "messages in the renderer or else deadlocks can occur if the page "
        "has windowed plugins! (message type " << message.type() << ")";
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    sender->Send(reply);
    return false;
  }
#endif
  return true;
}
