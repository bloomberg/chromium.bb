// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COOKIE_MESSAGE_FILTER_H_
#define CHROME_RENDERER_COOKIE_MESSAGE_FILTER_H_
#pragma once

#include "base/waitable_event.h"
#include "ipc/ipc_channel_proxy.h"

// This class maintains a WaitableEvent that is signaled when an IPC to query
// cookies from the browser should pump events.  Pumping events may be
// necessary to avoid deadlocks if the browser blocks the cookie query on a
// user prompt.
class CookieMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  CookieMessageFilter();

  base::WaitableEvent* pump_messages_event() { return &event_; }
  void ResetPumpMessagesEvent() { event_.Reset(); }

 private:
  // IPC::ChannelProxy::MessageFilter implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);

  base::WaitableEvent event_;
};

#endif  // CHROME_RENDERER_COOKIE_MESSAGE_FILTER_H_
