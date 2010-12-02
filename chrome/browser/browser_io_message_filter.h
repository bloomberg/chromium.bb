// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_IO_MESSAGE_FILTER_H_
#define CHROME_BROWSER_BROWSER_IO_MESSAGE_FILTER_H_
#pragma once

#include "base/process.h"
#include "ipc/ipc_channel_proxy.h"

// Base class for message filters in the browser process that reside on the IO
// thread.
class BrowserIOMessageFilter : public IPC::ChannelProxy::MessageFilter,
                               public IPC::Message::Sender {
 public:
  BrowserIOMessageFilter();
  virtual ~BrowserIOMessageFilter();

  // IPC::ChannelProxy::MessageFilter methods.  If you override them, make sure
  // to call them as well.
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnChannelClosing();
  virtual void OnChannelConnected(int32 peer_pid);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

 protected:
  base::ProcessHandle peer_handle() { return peer_handle_; }

 private:
  IPC::Channel* channel_;
  base::ProcessHandle peer_handle_;
};

#endif  // CHROME_BROWSER_BROWSER_IO_MESSAGE_FILTER_H_
