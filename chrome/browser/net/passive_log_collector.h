// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
#define CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_

#include <vector>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "net/base/net_log.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class PassiveLogCollector : public ChromeNetLog::Observer {
 public:
  // This structure encapsulates all of the parameters of a captured event,
  // including an "order" field that identifies when it was captured relative
  // to other events.
  struct Entry {
    Entry(uint32 order,
          net::NetLog::EventType type,
          const base::TimeTicks& time,
          net::NetLog::Source source,
          net::NetLog::EventPhase phase,
          net::NetLog::EventParameters* params)
        : order(order), type(type), time(time), source(source), phase(phase),
          params(params) {
    }

    uint32 order;
    net::NetLog::EventType type;
    base::TimeTicks time;
    net::NetLog::Source source;
    net::NetLog::EventPhase phase;
    scoped_refptr<net::NetLog::EventParameters> params;
  };

  typedef std::vector<Entry> EntryList;

  struct RequestInfo {
    RequestInfo()
        : source_id(net::NetLog::Source::kInvalidId),
          num_entries_truncated(0),
          total_bytes_transmitted(0),
          total_bytes_received(0),
          bytes_transmitted(0),
          bytes_received(0),
          last_tx_rx_position(0) {}
    uint32 source_id;
    EntryList entries;
    size_t num_entries_truncated;
    net::NetLog::Source subordinate_source;

    // Only used in RequestTracker.
    std::string url;

    // Only used in SocketTracker.
    uint64 total_bytes_transmitted;
    uint64 total_bytes_received;
    uint64 bytes_transmitted;
    uint64 bytes_received;
    uint32 last_tx_rx_position;  // The |order| of the last Tx or Rx entry.
    base::TimeTicks last_tx_rx_time;  // The |time| of the last Tx or Rx entry.
  };

  typedef std::vector<RequestInfo> RequestInfoList;

  // This class stores and manages the passively logged information for
  // URLRequests/SocketStreams/ConnectJobs.
  class RequestTrackerBase {
   public:
    explicit RequestTrackerBase(size_t max_graveyard_size);

    void OnAddEntry(const Entry& entry);

    RequestInfoList GetLiveRequests() const;
    void ClearRecentlyDeceased();
    RequestInfoList GetRecentlyDeceased() const;
    void SetUnbounded(bool unbounded);

    bool is_unbounded() const { return is_unbounded_; }

    void Clear();

    // Appends all the captured entries to |out|. The ordering is undefined.
    void AppendAllEntries(EntryList* out) const;

   protected:
    enum Action {
      ACTION_NONE,
      ACTION_DELETE,
      ACTION_MOVE_TO_GRAVEYARD,
    };

    // Updates |out_info| with the information from |entry|. Returns an action
    // to perform for this map entry on completion.
    virtual Action DoAddEntry(const Entry& entry, RequestInfo* out_info) = 0;

    // Finds a request, either in the live entries or the graveyard and returns
    // it.
    RequestInfo* GetRequestInfo(uint32 id);

    // When GetLiveRequests() is called, RequestTrackerBase calls this method
    // for each entry after adding it to the list which will be returned
    // to the caller.
    virtual void OnLiveRequest(RequestInfo* info) const {}

   private:
    typedef base::hash_map<uint32, RequestInfo> SourceIDToInfoMap;

    void RemoveFromLiveRequests(uint32 source_id);
    void InsertIntoGraveyard(const RequestInfo& info);

    SourceIDToInfoMap live_requests_;
    size_t max_graveyard_size_;
    size_t next_graveyard_index_;
    RequestInfoList graveyard_;
    bool is_unbounded_;

    DISALLOW_COPY_AND_ASSIGN(RequestTrackerBase);
  };

  // Specialization of RequestTrackerBase for handling ConnectJobs.
  class ConnectJobTracker : public RequestTrackerBase {
   public:
    static const size_t kMaxGraveyardSize;

    ConnectJobTracker();

    void AppendLogEntries(RequestInfo* out_info, bool unbounded,
                          uint32 connect_id);

   protected:
    virtual Action DoAddEntry(const Entry& entry, RequestInfo* out_info);
   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobTracker);
  };

  // Specialization of RequestTrackerBase for handling Sockets.
  class SocketTracker : public RequestTrackerBase {
   public:
    static const size_t kMaxGraveyardSize;

    SocketTracker();

    void AppendLogEntries(RequestInfo* out_info, bool unbounded,
                          uint32 socket_id, bool clear);

   protected:
    virtual Action DoAddEntry(const Entry& entry, RequestInfo* out_info);

   private:
    void ClearInfo(RequestInfo* info);

    DISALLOW_COPY_AND_ASSIGN(SocketTracker);
  };

  // Specialization of RequestTrackerBase for handling URLRequest/SocketStream.
  class RequestTracker : public RequestTrackerBase {
   public:
    static const size_t kMaxGraveyardSize;
    static const size_t kMaxGraveyardURLSize;

    RequestTracker(ConnectJobTracker* connect_job_tracker,
                   SocketTracker* socket_tracker);

    void IntegrateSubordinateSource(RequestInfo* info,
                                    bool clear_entries) const;

   protected:
    virtual Action DoAddEntry(const Entry& entry, RequestInfo* out_info);

    virtual void OnLiveRequest(RequestInfo* info) const {
      IntegrateSubordinateSource(info, false);
    }

   private:
    ConnectJobTracker* connect_job_tracker_;
    SocketTracker* socket_tracker_;

    DISALLOW_COPY_AND_ASSIGN(RequestTracker);
  };

  // Tracks the log entries for the last seen SOURCE_INIT_PROXY_RESOLVER.
  class InitProxyResolverTracker {
   public:
    InitProxyResolverTracker();

    void OnAddEntry(const Entry& entry);

    const EntryList& entries() const {
      return entries_;
    }

   private:
    EntryList entries_;
    DISALLOW_COPY_AND_ASSIGN(InitProxyResolverTracker);
  };

  PassiveLogCollector();
  ~PassiveLogCollector();

  // Observer implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params);

  // Clears all of the passively logged data.
  void Clear();

  RequestTracker* url_request_tracker() {
    return &url_request_tracker_;
  }

  RequestTracker* socket_stream_tracker() {
    return &socket_stream_tracker_;
  }

  InitProxyResolverTracker* init_proxy_resolver_tracker() {
    return &init_proxy_resolver_tracker_;
  }

  // Fills |out| with the full list of events that have been passively
  // captured. The list is ordered by capture time.
  void GetAllCapturedEvents(EntryList* out) const;

 private:
  FRIEND_TEST(PassiveLogCollectorTest, LostConnectJob);
  FRIEND_TEST(PassiveLogCollectorTest, LostSocket);

  ConnectJobTracker connect_job_tracker_;
  SocketTracker socket_tracker_;
  RequestTracker url_request_tracker_;
  RequestTracker socket_stream_tracker_;
  InitProxyResolverTracker init_proxy_resolver_tracker_;

  // The count of how many events have flowed through this log. Used to set the
  // "order" field on captured events.
  uint32 num_events_seen_;

  DISALLOW_COPY_AND_ASSIGN(PassiveLogCollector);
};

#endif  // CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
