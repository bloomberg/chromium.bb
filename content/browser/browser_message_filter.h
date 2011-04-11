// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_BROWSER_MESSAGE_FILTER_H_
#pragma once

#include "base/process.h"
#include "content/browser/browser_thread.h"
#include "ipc/ipc_channel_proxy.h"

// Base class for message filters in the browser process.  You can receive and
// send messages on any thread.
class BrowserMessageFilter : public IPC::ChannelProxy::MessageFilter,
                             public IPC::Message::Sender {
 public:
  BrowserMessageFilter();
  virtual ~BrowserMessageFilter();

  // IPC::ChannelProxy::MessageFilter methods.  If you override them, make sure
  // to call them as well.  These are always called on the IO thread.
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnChannelClosing();
  virtual void OnChannelConnected(int32 peer_pid);
  // DON'T OVERRIDE THIS!  Override the other version below.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender implementation.  Can be called on any thread.  Can't
  // send sync messages (since we don't want to block the browser on any other
  // process).
  virtual bool Send(IPC::Message* message);

  // If you want the given message to be dispatched to your OnMessageReceived on
  // a different thread, change |thread| to the id of the target thread.
  // If you don't handle this message, or want to keep it on the IO thread, do
  // nothing.
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread);

  // Override this to receive messages.
  // Your function will normally be called on the IO thread.  However, if your
  // OverrideThreadForMessage modifies the thread used to dispatch the message,
  // your function will be called on the requested thread.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) = 0;

  // Can be called on any thread, after OnChannelConnected is called.
  base::ProcessHandle peer_handle() { return peer_handle_; }

 protected:
  // Call this if a message couldn't be deserialized.  This kills the renderer.
  // Can be called on any thread.
  virtual void BadMessageReceived();

 private:
  // Dispatches a message to the derived class.
  bool DispatchMessage(const IPC::Message& message);

  IPC::Channel* channel_;
  base::ProcessHandle peer_handle_;
};

#endif  // CONTENT_BROWSER_BROWSER_MESSAGE_FILTER_H_
