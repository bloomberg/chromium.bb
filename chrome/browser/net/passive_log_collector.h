// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
#define CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_

#include <vector>

#include "base/hash_tables.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "net/base/net_log.h"

class PassiveLogCollector : public ChromeNetLog::Observer {
 public:
  struct RequestInfo {
    RequestInfo() : num_entries_truncated(0) {}
    std::string url;
    std::vector<net::NetLog::Entry> entries;
    size_t num_entries_truncated;
  };

  typedef std::vector<RequestInfo> RequestInfoList;

  // This class stores and manages the passively logged information for
  // URLRequests/SocketStreams/ConnectJobs.
  class RequestTrackerBase {
   public:
    explicit RequestTrackerBase(size_t max_graveyard_size);

    void OnAddEntry(const net::NetLog::Entry& entry);

    RequestInfoList GetLiveRequests() const;
    void ClearRecentlyDeceased();
    RequestInfoList GetRecentlyDeceased() const;
    void SetUnbounded(bool unbounded);

    bool IsUnbounded() const { return is_unbounded_; }

    void Clear();

    const RequestInfo* GetRequestInfoFromGraveyard(int id) const;

   protected:
    enum Action {
      ACTION_NONE,
      ACTION_DELETE,
      ACTION_MOVE_TO_GRAVEYARD,
    };

    // Updates |out_info| with the information from |entry|. Returns an action
    // to perform for this map entry on completion.
    virtual Action DoAddEntry(const net::NetLog::Entry& entry,
                              RequestInfo* out_info) = 0;

    bool is_unbounded() const { return is_unbounded_; }

   private:
    typedef base::hash_map<int, RequestInfo> SourceIDToInfoMap;

    bool HandleNotificationOfConnectJobID(const net::NetLog::Entry& entry,
                                          RequestInfo* live_entry);

    void RemoveFromLiveRequests(const RequestInfo& info);
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
    virtual Action DoAddEntry(const net::NetLog::Entry& entry,
                              RequestInfo* out_info);
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
    virtual Action DoAddEntry(const net::NetLog::Entry& entry,
                              RequestInfo* out_info);

   private:
    // Searches through |connect_job_tracker_| for information on the
    // ConnectJob specified in |entry|, and appends it to |live_entry|.
    void AddConnectJobInfo(const net::NetLog::Entry& entry,
                           RequestInfo* live_entry);

    ConnectJobTracker* connect_job_tracker_;

    DISALLOW_COPY_AND_ASSIGN(RequestTracker);
  };

  PassiveLogCollector();
  ~PassiveLogCollector();

  // Observer implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry);

  // Clears all of the passively logged data.
  void Clear();

  RequestTracker* url_request_tracker() {
    return &url_request_tracker_;
  }

  RequestTracker* socket_stream_tracker() {
    return &socket_stream_tracker_;
  }

 private:
  ConnectJobTracker connect_job_tracker_;
  RequestTracker url_request_tracker_;
  RequestTracker socket_stream_tracker_;

  DISALLOW_COPY_AND_ASSIGN(PassiveLogCollector);
};

#endif  // CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
