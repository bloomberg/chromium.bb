// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_DATA_REMOVER_H_
#define CHROME_BROWSER_PLUGIN_DATA_REMOVER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "content/browser/plugin_process_host.h"

class Task;

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

class PluginDataRemover : public base::RefCountedThreadSafe<PluginDataRemover>,
                          public PluginProcessHost::Client,
                          public IPC::Channel::Listener {
 public:
  PluginDataRemover();

  // The plug-in whose data should be removed (usually Flash) is specified via
  // its MIME type. This method sets a different MIME type in order to call a
  // different plug-in (for example in tests).
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

  // Starts removing plug-in data stored since |begin_time|.
  base::WaitableEvent* StartRemoving(base::Time begin_time);

  // Returns whether there is a plug-in installed that supports removing
  // LSO data. Because this method possibly has to load the plug-in list, it
  // should only be called on the FILE thread.
  static bool IsSupported();

  // Indicates whether we are still in the process of removing plug-in data.
  bool is_removing() const { return is_removing_; }

  // Wait until removing has finished. When the browser is still running (i.e.
  // not during shutdown), you should use a WaitableEventWatcher in combination
  // with the WaitableEvent returned by StartRemoving.
  void Wait();

  // PluginProcessHost::Client methods.
  virtual int ID();
  virtual bool OffTheRecord();
  virtual void SetPluginInfo(const webkit::npapi::WebPluginInfo& info);
  virtual void OnChannelOpened(const IPC::ChannelHandle& handle);
  virtual void OnError();

  // IPC::Channel::Listener methods.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

 private:
  friend class base::RefCountedThreadSafe<PluginDataRemover>;
  friend class PluginDataRemoverTest;
  ~PluginDataRemover();

  // Signals that we are finished with removing data (successful or not). This
  // method is safe to call multiple times.
  void SignalDone();
  // Connects the client side of a newly opened plug-in channel.
  void ConnectToChannel(const IPC::ChannelHandle& handle);
  // Handles the PluginHostMsg_ClearSiteDataResult message.
  void OnClearSiteDataResult(bool success);
  // Called when a timeout happens in order not to block the client
  // indefinitely.
  void OnTimeout();

  std::string mime_type_;
  bool is_removing_;
  // The point in time when we start removing data.
  base::Time remove_start_time_;
  // The point in time from which on we remove data.
  base::Time begin_time_;
  scoped_ptr<base::WaitableEvent> event_;
  // We own the channel, but it's used on the IO thread, so it needs to be
  // deleted there. It's NULL until we have opened a connection to the plug-in
  // process.
  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(PluginDataRemover);
};

#endif  // CHROME_BROWSER_PLUGIN_DATA_REMOVER_H_
