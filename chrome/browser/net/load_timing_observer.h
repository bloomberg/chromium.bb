// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_LOAD_TIMING_OBSERVER_H_
#define CHROME_BROWSER_NET_LOAD_TIMING_OBSERVER_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/time.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "net/base/net_log.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace net {
class URLRequest;
}  // namespace net

struct ResourceResponse;

// LoadTimingObserver watches the NetLog event stream and collects the network
// timing information.
//
// LoadTimingObserver lives completely on the IOThread and ignores events from
// other threads.  It is not safe to use from other threads.
class LoadTimingObserver : public ChromeNetLog::ThreadSafeObserverImpl {
 public:
  struct URLRequestRecord {
    URLRequestRecord();

    webkit_glue::ResourceLoadTimingInfo timing;
    uint32 connect_job_id;
    uint32 socket_log_id;
    bool socket_reused;
    base::TimeTicks base_ticks;
  };

  struct HTTPStreamJobRecord {
    HTTPStreamJobRecord();

    uint32 socket_log_id;
    bool socket_reused;
    base::TimeTicks connect_start;
    base::TimeTicks connect_end;
    base::TimeTicks dns_start;
    base::TimeTicks dns_end;
    base::TimeTicks ssl_start;
    base::TimeTicks ssl_end;
  };

  struct ConnectJobRecord {
    base::TimeTicks dns_start;
    base::TimeTicks dns_end;
  };

  struct SocketRecord {
    base::TimeTicks ssl_start;
    base::TimeTicks ssl_end;
  };

  LoadTimingObserver();
  virtual ~LoadTimingObserver();

  URLRequestRecord* GetURLRequestRecord(uint32 source_id);

  // ThreadSafeObserver implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params) OVERRIDE;

  static void PopulateTimingInfo(net::URLRequest* request,
                                 ResourceResponse* response);

 private:
  FRIEND_TEST_ALL_PREFIXES(LoadTimingObserverTest,
                           HTTPStreamJobRecord);
  FRIEND_TEST_ALL_PREFIXES(LoadTimingObserverTest,
                           ConnectJobRecord);
  FRIEND_TEST_ALL_PREFIXES(LoadTimingObserverTest,
                           SocketRecord);

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

  void OnAddConnectJobEntry(net::NetLog::EventType type,
                            const base::TimeTicks& time,
                            const net::NetLog::Source& source,
                            net::NetLog::EventPhase phase,
                            net::NetLog::EventParameters* params);

  void OnAddSocketEntry(net::NetLog::EventType type,
                        const base::TimeTicks& time,
                        const net::NetLog::Source& source,
                        net::NetLog::EventPhase phase,
                        net::NetLog::EventParameters* params);

  URLRequestRecord* CreateURLRequestRecord(uint32 source_id);
  void DeleteURLRequestRecord(uint32 source_id);

  typedef base::hash_map<uint32, URLRequestRecord> URLRequestToRecordMap;
  typedef base::hash_map<uint32, HTTPStreamJobRecord> HTTPStreamJobToRecordMap;
  typedef base::hash_map<uint32, ConnectJobRecord> ConnectJobToRecordMap;
  typedef base::hash_map<uint32, SocketRecord> SocketToRecordMap;
  URLRequestToRecordMap url_request_to_record_;
  HTTPStreamJobToRecordMap http_stream_job_to_record_;
  ConnectJobToRecordMap connect_job_to_record_;
  SocketToRecordMap socket_to_record_;
  uint32 last_connect_job_id_;
  ConnectJobRecord last_connect_job_record_;

  DISALLOW_COPY_AND_ASSIGN(LoadTimingObserver);
};

#endif  // CHROME_BROWSER_NET_LOAD_TIMING_OBSERVER_H_
