// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_DATA_REMOVER_IMPL_H_
#define CONTENT_BROWSER_PLUGIN_DATA_REMOVER_IMPL_H_
#pragma once

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/browser/plugin_process_host.h"
#include "content/public/browser/plugin_data_remover.h"

class CONTENT_EXPORT PluginDataRemoverImpl : public content::PluginDataRemover,
                                             public PluginProcessHost::Client,
                                             public IPC::Channel::Listener {
 public:
  explicit PluginDataRemoverImpl(
      const content::ResourceContext& resource_context);
  virtual ~PluginDataRemoverImpl();

  // content::PluginDataRemover implementation:
  virtual base::WaitableEvent* StartRemoving(base::Time begin_time) OVERRIDE;

  // The plug-in whose data should be removed (usually Flash) is specified via
  // its MIME type. This method sets a different MIME type in order to call a
  // different plug-in (for example in tests).
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

  // PluginProcessHost::Client methods.
  virtual int ID() OVERRIDE;
  virtual bool OffTheRecord() OVERRIDE;
  virtual const content::ResourceContext& GetResourceContext() OVERRIDE;
  virtual void SetPluginInfo(const webkit::WebPluginInfo& info) OVERRIDE;
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE;
  virtual void OnSentPluginChannelRequest() OVERRIDE;
  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) OVERRIDE;
  virtual void OnError() OVERRIDE;

  // IPC::Channel::Listener methods.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
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
  bool is_starting_process_;
  bool is_removing_;
  // The point in time when we start removing data.
  base::Time remove_start_time_;
  // The point in time from which on we remove data.
  base::Time begin_time_;
  // The resource context for the profile.
  const content::ResourceContext& context_;
  scoped_ptr<base::WaitableEvent> event_;
  // We own the channel, but it's used on the IO thread, so it needs to be
  // deleted there. It's NULL until we have opened a connection to the plug-in
  // process.
  IPC::Channel* channel_;

  base::WeakPtrFactory<PluginDataRemoverImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginDataRemoverImpl);
};

#endif  // CONTENT_BROWSER_PLUGIN_DATA_REMOVER_IMPL_H_
