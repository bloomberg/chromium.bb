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

class PassiveLogCollector : public ChromeNetLog::Observer {
 public:
  // This structure encapsulates all of the parameters of a captured event,
  // including an "order" field that identifies when it was captured relative
  // to other events.
  struct Entry {
    Entry(int order,
          net::NetLog::EventType type,
          const base::TimeTicks& time,
          net::NetLog::Source source,
          net::NetLog::EventPhase phase,
          net::NetLog::EventParameters* extra_parameters)
        : order(order), type(type), time(time), source(source), phase(phase),
          extra_parameters(extra_parameters) {
    }

    int order;
    net::NetLog::EventType type;
    base::TimeTicks time;
    net::NetLog::Source source;
    net::NetLog::EventPhase phase;
    scoped_refptr<net::NetLog::EventParameters> extra_parameters;
  };

  typedef std::vector<Entry> EntryList;

  struct RequestInfo {
    RequestInfo() : num_entries_truncated(0) {}
    std::string url;
    EntryList entries;
    size_t num_entries_truncated;
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

    bool IsUnbounded() const { return is_unbounded_; }

    void Clear();

    // Appends all the captured entries to |out|. The ordering is undefined.
    void AppendAllEntries(EntryList* out) const;

    const RequestInfo* GetRequestInfoFromGraveyard(int id) const;

   protected:
    enum Action {
      ACTION_NONE,
      ACTION_DELETE,
      ACTION_MOVE_TO_GRAVEYARD,
    };

    // Updates |out_info| with the information from |entry|. Returns an action
    // to perform for this map entry on completion.
    virtual Action DoAddEntry(const Entry& entry, RequestInfo* out_info) = 0;

    bool is_unbounded() const { return is_unbounded_; }

   private:
    typedef base::hash_map<int, RequestInfo> SourceIDToInfoMap;

    void RemoveFromLiveRequests(int source_id);
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

   protected:
    virtual Action DoAddEntry(const Entry& entry, RequestInfo* out_info);
   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobTracker);
  };

  // Specialization of RequestTrackerBase for handling URLRequest/SocketStream.
  class RequestTracker : public RequestTrackerBase {
   public:
    static const size_t kMaxGraveyardSize;
    static const size_t kMaxGraveyardURLSize;

    explicit RequestTracker(ConnectJobTracker* connect_job_tracker);

   protected:
    virtual Action DoAddEntry(const Entry& entry, RequestInfo* out_info);

   private:
    // Searches through |connect_job_tracker_| for information on the
    // ConnectJob specified in |entry|, and appends it to |live_entry|.
    void AddConnectJobInfo(const Entry& entry, RequestInfo* live_entry);

    ConnectJobTracker* connect_job_tracker_;

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
                          net::NetLog::EventParameters* extra_parameters);

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
  ConnectJobTracker connect_job_tracker_;
  RequestTracker url_request_tracker_;
  RequestTracker socket_stream_tracker_;
  InitProxyResolverTracker init_proxy_resolver_tracker_;

  // The count of how many events have flowed through this log. Used to set the
  // "order" field on captured events.
  int num_events_seen_;

  DISALLOW_COPY_AND_ASSIGN(PassiveLogCollector);
};

#endif  // CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
