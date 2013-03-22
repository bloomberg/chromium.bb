// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "net/socket/tcp_client_socket.h"

namespace base {
class Thread;
class DictionaryValue;
}

class DevToolsAdbBridge {
 public:
  typedef base::Callback<void(int result,
                              const std::string& response)> Callback;

  class AgentHost : public base::RefCounted<AgentHost> {
   public:
    AgentHost(const std::string& serial,
              const std::string& model,
              const base::DictionaryValue& value);

    std::string serial() { return serial_; }
    std::string model() { return model_; }
    std::string id() { return id_; }
    std::string title() { return title_; }
    std::string description() { return description_; }
    std::string favicon_url() { return favicon_url_; }
    std::string debug_url() { return debug_url_; }

   private:
    friend class base::RefCounted<AgentHost>;

    virtual ~AgentHost();
    std::string serial_;
    std::string model_;
    std::string id_;
    std::string title_;
    std::string description_;
    std::string favicon_url_;
    std::string debug_url_;
    DISALLOW_COPY_AND_ASSIGN(AgentHost);
  };

  typedef std::vector<scoped_refptr<AgentHost> > AgentHosts;
  typedef base::Callback<void(int, AgentHosts*)> HostsCallback;

  static DevToolsAdbBridge* Start();
  void Query(const std::string query, const Callback& callback);
  void Devices();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<DevToolsAdbBridge>;


  explicit DevToolsAdbBridge();
  virtual ~DevToolsAdbBridge();

  void StopHandlerOnFileThread();

  void ResetHandlerAndReleaseOnUIThread();
  void ResetHandlerOnUIThread();

  void QueryOnHandlerThread(const std::string query, const Callback& callback);
  void QueryResponseOnHandlerThread(const Callback& callback,
                                    int result,
                                    const std::string& response);

  void DevicesOnHandlerThread(const HostsCallback& callback);
  void ReceivedDevices(const HostsCallback& callback,
                       int result,
                       const std::string& response);
  void ProcessSerials(const HostsCallback& callback,
                      AgentHosts* hosts,
                      std::vector<std::string>* serials);
  void ReceivedModel(const HostsCallback& callback,
                     AgentHosts* hosts,
                     std::vector<std::string>* serials,
                     int result,
                     const std::string& response);
  void ReceivedPages(const HostsCallback& callback,
                     AgentHosts* hosts,
                     std::vector<std::string>* serials,
                     const std::string& model,
                     int result,
                     const std::string& response);

  void RespondOnUIThread(const Callback& callback,
                         int result,
                         const std::string& response);

  void PrintHosts(int result, AgentHosts* hosts);

  // The thread used by the devtools to run client socket.
  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAdbBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
