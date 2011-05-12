// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_WORKER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_CHROME_WORKER_MESSAGE_FILTER_H_
#pragma once

#include "base/string16.h"
#include "ipc/ipc_channel_proxy.h"

class GURL;
class HostContentSettingsMap;
class WorkerProcessHost;

// This class filters out incoming Chrome-specific IPC messages for the renderer
// process on the IPC thread.
class ChromeWorkerMessageFilter : public IPC::ChannelProxy::MessageFilter,
                                  public IPC::Message::Sender {
 public:
  explicit ChromeWorkerMessageFilter(WorkerProcessHost* process);

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender methods:
  virtual bool Send(IPC::Message* message);

 private:
  virtual ~ChromeWorkerMessageFilter();

  void OnAllowDatabase(int worker_route_id,
                       const GURL& url,
                       const string16& name,
                       const string16& display_name,
                       unsigned long estimated_size,
                       bool* result);
  void OnAllowFileSystem(int worker_route_id,
                         const GURL& url,
                         bool* result);

  WorkerProcessHost* process_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWorkerMessageFilter);
};

#endif  // CHROME_BROWSER_CHROME_WORKER_MESSAGE_FILTER_H_
