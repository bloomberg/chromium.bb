// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_NETLOG_OBSERVER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_NETLOG_OBSERVER_H_
#pragma once

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace net {
class URLRequest;
}  // namespace net

class IOThread;
struct ResourceResponse;

// DevToolsNetLogObserver watches the NetLog event stream and collects the
// stuff that may be of interest to DevTools. Currently, this only includes
// actual HTTP/SPDY headers sent and received over the network.
//
// As DevToolsNetLogObserver shares live data with objects that live on the
// IO Thread, it must also reside on the IO Thread.  Only OnAddEntry can be
// called from other threads.
class DevToolsNetLogObserver: public ChromeNetLog::ThreadSafeObserver {
  typedef webkit_glue::ResourceDevToolsInfo ResourceInfo;

 public:
  // ThreadSafeObserver implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params);

  void OnAddURLRequestEntry(net::NetLog::EventType type,
                            const base::TimeTicks& time,
                            const net::NetLog::Source& source,
                            net::NetLog::EventPhase phase,
                            net::NetLog::EventParameters* params);

  void OnAddHTTPStreamJobEntry(net::NetLog::EventType type,
                               const base::TimeTicks& time,
                               const net::NetLog::Source& source,
                               net::NetLog::EventPhase phase,
                               net::NetLog::EventParameters* params);

  void OnAddSocketEntry(net::NetLog::EventType type,
                        const base::TimeTicks& time,
                        const net::NetLog::Source& source,
                        net::NetLog::EventPhase phase,
                        net::NetLog::EventParameters* params);

  static void Attach(IOThread* thread);
  static void Detach();

  // Must be called on the IO thread. May return NULL if no observers
  // are active.
  static DevToolsNetLogObserver* GetInstance();
  static void PopulateResponseInfo(net::URLRequest*, ResourceResponse*);
  static int GetAndResetEncodedDataLength(net::URLRequest* request);

 private:
  static DevToolsNetLogObserver* instance_;

  explicit DevToolsNetLogObserver(ChromeNetLog* chrome_net_log);
  ~DevToolsNetLogObserver();

  ResourceInfo* GetResourceInfo(uint32 id);

  ChromeNetLog* chrome_net_log_;
  typedef base::hash_map<uint32, scoped_refptr<ResourceInfo> > RequestToInfoMap;
  typedef base::hash_map<uint32, int> RequestToEncodedDataLengthMap;
  typedef base::hash_map<uint32, uint32> HTTPStreamJobToSocketMap;
  typedef base::hash_map<uint32, uint32> SocketToRequestMap;
  RequestToInfoMap request_to_info_;
  RequestToEncodedDataLengthMap request_to_encoded_data_length_;
  HTTPStreamJobToSocketMap http_stream_job_to_socket_;
  SocketToRequestMap socket_to_request_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetLogObserver);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_NETLOG_OBSERVER_H_
