// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging_message_filter.h"

#include <string>

#include "content/common/push_messaging_messages.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PushMessagingMessageFilter::PushMessagingMessageFilter()
    : BrowserMessageFilter(PushMessagingMsgStart) {}

PushMessagingMessageFilter::~PushMessagingMessageFilter() {}

bool PushMessagingMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                   bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PushMessagingMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_Register, OnRegister)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingMessageFilter::OnRegister(int routing_id,
                                            int callbacks_id,
                                            const std::string& sender_id) {
  // TODO(mvanouwerkerk): Really implement, the below simply returns an error.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GURL endpoint = GURL("https://android.googleapis.com/gcm/send");
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&PushMessagingMessageFilter::DidRegister,
                                     this,
                                     routing_id,
                                     callbacks_id,
                                     endpoint,
                                     "",
                                     true));

}

void PushMessagingMessageFilter::DidRegister(int routing_id,
                                             int callbacks_id,
                                             const GURL& endpoint,
                                             const std::string& registration_id,
                                             bool error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!error) {
    Send(new PushMessagingMsg_RegisterSuccess(routing_id,
                                              callbacks_id,
                                              endpoint,
                                              registration_id));
  } else {
    Send(new PushMessagingMsg_RegisterError(routing_id, callbacks_id));
  }
}

}  // namespace content
