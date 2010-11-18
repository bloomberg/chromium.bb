// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_DATA_REMOVER_H_
#define CHROME_BROWSER_PLUGIN_DATA_REMOVER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/plugin_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"

class Task;

namespace base {
class MessageLoopProxy;
}

class PluginDataRemover : public PluginProcessHost::Client,
                          public IPC::Channel::Listener {
 public:
  PluginDataRemover();
  ~PluginDataRemover();

  // Starts removing plug-in data stored since |begin_time| and calls
  // |done_task| on the current thread when finished.
  void StartRemoving(base::Time begin_time, Task* done_task);

  // PluginProcessHost::Client methods
  virtual int ID();
  virtual bool OffTheRecord();
  virtual void SetPluginInfo(const WebPluginInfo& info);
  virtual void OnChannelOpened(const IPC::ChannelHandle& handle);
  virtual void OnError();

  // IPC::ChannelProxy::MessageFilter methods
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

 private:
  void SignalDone();
  void SignalError();
  void OnClearSiteDataResult(bool success);
  void OnTimeout();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_ptr<Task> done_task_;
  base::Time begin_time_;
  // We own the channel, but it's used on the IO thread, so it needs to be
  // deleted there as well.
  IPC::Channel* channel_;
  ScopedRunnableMethodFactory<PluginDataRemover> method_factory_;
};

#endif  // CHROME_BROWSER_PLUGIN_DATA_REMOVER_H_
