// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_
#define CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_
#pragma once

#include <queue>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/pepper_message_filter.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "ipc/ipc_message.h"

class BrowserChildProcessHostImpl;

namespace content {
struct PepperPluginInfo;
class ResourceContext;
}

namespace net {
class HostResolver;
}

// Process host for PPAPI plugin and broker processes.
// When used for the broker, interpret all references to "plugin" with "broker".
class PpapiPluginProcessHost : public content::BrowserChildProcessHostDelegate,
                               public IPC::Message::Sender {
 public:
  class Client {
   public:
    // Gets the information about the renderer that's requesting the channel.
    virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                     int* renderer_id) = 0;

    // Called when the channel is asynchronously opened to the plugin or on
    // error. On error, the parameters should be:
    //   base::kNullProcessHandle
    //   IPC::ChannelHandle(),
    //   0
    virtual void OnPpapiChannelOpened(
        const IPC::ChannelHandle& channel_handle,
        int plugin_child_id) = 0;

    // Returns true if the current connection is off-the-record.
    virtual bool OffTheRecord() = 0;
  };

  class PluginClient : public Client {
   public:
    // Returns the resource context for the renderer requesting the channel.
    virtual content::ResourceContext* GetResourceContext() = 0;
  };

  class BrokerClient : public Client {
  };

  virtual ~PpapiPluginProcessHost();

  static PpapiPluginProcessHost* CreatePluginHost(
      const content::PepperPluginInfo& info,
      net::HostResolver* host_resolver);
  static PpapiPluginProcessHost* CreateBrokerHost(
      const content::PepperPluginInfo& info);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // Opens a new channel to the plugin. The client will be notified when the
  // channel is ready or if there's an error.
  void OpenChannelToPlugin(Client* client);

  const FilePath& plugin_path() const { return plugin_path_; }

  // The client pointer must remain valid until its callback is issued.

 private:
  class PluginNetworkObserver;

  // Constructors for plugin and broker process hosts, respectively.
  // You must call Init before doing anything else.
  PpapiPluginProcessHost(net::HostResolver* host_resolver);
  PpapiPluginProcessHost();

  // Actually launches the process with the given plugin info. Returns true
  // on success (the process was spawned).
  bool Init(const content::PepperPluginInfo& info);

  void RequestPluginChannel(Client* client);

  virtual void OnProcessLaunched() OVERRIDE;

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  void CancelRequests();

  // IPC message handlers.
  void OnRendererPluginChannelCreated(const IPC::ChannelHandle& handle);

  // Handles most requests from the plugin. May be NULL.
  scoped_refptr<PepperMessageFilter> filter_;

  // Observes network changes. May be NULL.
  scoped_ptr<PluginNetworkObserver> network_observer_;

  // Channel requests that we are waiting to send to the plugin process once
  // the channel is opened.
  std::vector<Client*> pending_requests_;

  // Channel requests that we have already sent to the plugin process, but
  // haven't heard back about yet.
  std::queue<Client*> sent_requests_;

  // Path to the plugin library.
  FilePath plugin_path_;

  const bool is_broker_;

  scoped_ptr<BrowserChildProcessHostImpl> process_;

  DISALLOW_COPY_AND_ASSIGN(PpapiPluginProcessHost);
};

class PpapiPluginProcessHostIterator
    : public content::BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost> {
 public:
  PpapiPluginProcessHostIterator()
      : content::BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost>(content::PROCESS_TYPE_PPAPI_PLUGIN) {}
};

class PpapiBrokerProcessHostIterator
    : public content::BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost> {
 public:
  PpapiBrokerProcessHostIterator()
      : content::BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost>(content::PROCESS_TYPE_PPAPI_BROKER) {}
};

#endif  // CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_

