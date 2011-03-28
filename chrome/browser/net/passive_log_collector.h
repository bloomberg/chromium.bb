// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
#define CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "net/base/net_log.h"

// PassiveLogCollector watches the NetLog event stream, and saves the network
// events for recent requests, in a circular buffer.
//
// This is done so that when a network problem is encountered (performance
// problem, or error), about:net-internals can be opened shortly after the
// problem and it will contain a trace for the problem request.
//
// (This is in contrast to the "active logging" which captures every single
// network event, but requires capturing to have been enabled *prior* to
// encountering the problem. Active capturing is enabled as long as
// about:net-internals is open).
//
// The data captured by PassiveLogCollector is grouped by NetLog::Source, into
// a SourceInfo structure. These in turn are grouped by NetLog::SourceType, and
// owned by a SourceTracker instance for the specific source type.
//
// The PassiveLogCollector is owned by the ChromeNetLog itself, and is not
// thread safe.  The ChromeNetLog is responsible for calling it in a thread safe
// manner.
class PassiveLogCollector : public ChromeNetLog::ThreadSafeObserver {
 public:
  typedef std::vector<net::NetLog::Source> SourceDependencyList;

  struct SourceInfo {
    SourceInfo();
    ~SourceInfo();

    // Returns the URL that corresponds with this source. This is
    // only meaningful for certain source types (URL_REQUEST, SOCKET_STREAM).
    // For the rest, it will return an empty string.
    std::string GetURL() const;

    uint32 source_id;
    ChromeNetLog::EntryList entries;
    size_t num_entries_truncated;

    // List of other sources which contain information relevant to this
    // source (for example, a url request might depend on the log items
    // for a connect job and for a socket that were bound to it.)
    SourceDependencyList dependencies;

    // Holds the count of how many other sources have added this as a
    // dependent source. When it is 0, it means noone has referenced it so it
    // can be deleted normally.
    int reference_count;

    // |is_alive| is set to false once the source has been added to the
    // tracker's graveyard (it may still be kept around due to a non-zero
    // reference_count, but it is still considered "dead").
    bool is_alive;
  };

  typedef std::vector<SourceInfo> SourceInfoList;

  // Interface for consuming a NetLog entry.
  class SourceTrackerInterface {
   public:
    virtual ~SourceTrackerInterface() {}

    virtual void OnAddEntry(const ChromeNetLog::Entry& entry) = 0;

    // Clears all the passively logged data from this tracker.
    virtual void Clear() = 0;

    // Appends all the captured entries to |out|. The ordering is undefined.
    virtual void AppendAllEntries(ChromeNetLog::EntryList* out) const = 0;
  };

  // This source tracker is intended for TYPE_NONE. All entries go into a
  // circular buffer, and there is no concept of live/dead requests.
  class GlobalSourceTracker : public SourceTrackerInterface {
   public:
    GlobalSourceTracker();
    ~GlobalSourceTracker();

    // SourceTrackerInterface implementation:
    virtual void OnAddEntry(const ChromeNetLog::Entry& entry);
    virtual void Clear();
    virtual void AppendAllEntries(ChromeNetLog::EntryList* out) const;

   private:
    typedef std::deque<ChromeNetLog::Entry> CircularEntryList;
    CircularEntryList entries_;
    DISALLOW_COPY_AND_ASSIGN(GlobalSourceTracker);
  };

  // This class stores and manages the passively logged information for
  // URLRequests/SocketStreams/ConnectJobs.
  class SourceTracker : public SourceTrackerInterface {
   public:
    // Creates a SourceTracker that will track at most |max_num_sources|.
    // Up to |max_graveyard_size| unreferenced sources will be kept around
    // before deleting them for good. |parent| may be NULL, and points to
    // the owning PassiveLogCollector (it is used when adding references
    // to other sources).
    SourceTracker(size_t max_num_sources,
                  size_t max_graveyard_size,
                  PassiveLogCollector* parent);

    virtual ~SourceTracker();

    // SourceTrackerInterface implementation:
    virtual void OnAddEntry(const ChromeNetLog::Entry& entry);
    virtual void Clear();
    virtual void AppendAllEntries(ChromeNetLog::EntryList* out) const;

#ifdef UNIT_TEST
    // Helper used to inspect the current state by unit-tests.
    // Retuns a copy of the source infos held by the tracker.
    SourceInfoList GetAllDeadOrAliveSources(bool is_alive) const {
      SourceInfoList result;
      for (SourceIDToInfoMap::const_iterator it = sources_.begin();
           it != sources_.end(); ++it) {
        if (it->second.is_alive == is_alive)
          result.push_back(it->second);
      }
      return result;
    }
#endif

   protected:
    enum Action {
      ACTION_NONE,
      ACTION_DELETE,
      ACTION_MOVE_TO_GRAVEYARD,
    };

    // Makes |info| hold a reference to |source|. This way |source| will be
    // kept alive at least as long as |info|.
    void AddReferenceToSourceDependency(const net::NetLog::Source& source,
                                        SourceInfo* info);

   private:
    typedef base::hash_map<uint32, SourceInfo> SourceIDToInfoMap;
    typedef std::deque<uint32> DeletionQueue;

    // Updates |out_info| with the information from |entry|. Returns an action
    // to perform for this map entry on completion.
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info) = 0;

    // Removes |source_id| from |sources_|. This also releases any references
    // to dependencies held by this source.
    void DeleteSourceInfo(uint32 source_id);

    // Adds |source_id| to the FIFO queue (graveyard) for deletion.
    void AddToDeletionQueue(uint32 source_id);

    // Removes |source_id| from the |deletion_queue_| container.
    void EraseFromDeletionQueue(uint32 source_id);

    // Adds/Releases a reference from the source with ID |source_id|.
    // Use |offset=-1| to do a release, and |offset=1| for an addref.
    void AdjustReferenceCountForSource(int offset, uint32 source_id);

    // Releases all the references to sources held by |info|.
    void ReleaseAllReferencesToDependencies(SourceInfo* info);

    // This map contains all of the sources being tracked by this tracker.
    // (It includes both the "live" sources, and the "dead" ones.)
    SourceIDToInfoMap sources_;

    size_t max_num_sources_;
    size_t max_graveyard_size_;

    // FIFO queue for entries in |sources_| that are no longer alive, and
    // can be deleted. This buffer is also called "graveyard" elsewhere. We
    // queue sources for deletion so they can persist a bit longer.
    DeletionQueue deletion_queue_;

    PassiveLogCollector* parent_;

    DISALLOW_COPY_AND_ASSIGN(SourceTracker);
  };

  // Specialization of SourceTracker for handling ConnectJobs.
  class ConnectJobTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    explicit ConnectJobTracker(PassiveLogCollector* parent);

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);
    DISALLOW_COPY_AND_ASSIGN(ConnectJobTracker);
  };

  // Specialization of SourceTracker for handling Sockets.
  class SocketTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    SocketTracker();

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

    DISALLOW_COPY_AND_ASSIGN(SocketTracker);
  };

  // Specialization of SourceTracker for handling net::URLRequest/SocketStream.
  class RequestTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    explicit RequestTracker(PassiveLogCollector* parent);

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

    DISALLOW_COPY_AND_ASSIGN(RequestTracker);
  };

  // Specialization of SourceTracker for handling
  // SOURCE_INIT_PROXY_RESOLVER.
  class InitProxyResolverTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    InitProxyResolverTracker();

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

    DISALLOW_COPY_AND_ASSIGN(InitProxyResolverTracker);
  };

  // Tracks the log entries for the last seen SOURCE_SPDY_SESSION.
  class SpdySessionTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    SpdySessionTracker();

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

    DISALLOW_COPY_AND_ASSIGN(SpdySessionTracker);
  };

  // Tracks the log entries for the last seen SOURCE_HOST_RESOLVER_IMPL_REQUEST.
  class DNSRequestTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    DNSRequestTracker();

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

    DISALLOW_COPY_AND_ASSIGN(DNSRequestTracker);
  };

  // Tracks the log entries for the last seen SOURCE_HOST_RESOLVER_IMPL_JOB.
  class DNSJobTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    DNSJobTracker();

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

    DISALLOW_COPY_AND_ASSIGN(DNSJobTracker);
  };

  // Tracks the log entries for the last seen SOURCE_DISK_CACHE_ENTRY.
  class DiskCacheEntryTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    DiskCacheEntryTracker();

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

    DISALLOW_COPY_AND_ASSIGN(DiskCacheEntryTracker);
  };

  // Tracks the log entries for the last seen SOURCE_DISK_CACHE_ENTRY.
  class MemCacheEntryTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    MemCacheEntryTracker();

   protected:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);

   private:
    DISALLOW_COPY_AND_ASSIGN(MemCacheEntryTracker);
  };

  class HttpStreamJobTracker : public SourceTracker {
   public:
    static const size_t kMaxNumSources;
    static const size_t kMaxGraveyardSize;

    explicit HttpStreamJobTracker(PassiveLogCollector* parent);

   private:
    virtual Action DoAddEntry(const ChromeNetLog::Entry& entry,
                              SourceInfo* out_info);
    DISALLOW_COPY_AND_ASSIGN(HttpStreamJobTracker);
  };


  PassiveLogCollector();
  ~PassiveLogCollector();

  // ThreadSafeObserver implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params);

  // Clears all of the passively logged data.
  void Clear();

  // Fills |out| with the full list of events that have been passively
  // captured. The list is ordered by capture time.
  void GetAllCapturedEvents(ChromeNetLog::EntryList* out) const;

 private:
  // Returns the tracker to use for sources of type |source_type|, or NULL.
  SourceTrackerInterface* GetTrackerForSourceType(
      net::NetLog::SourceType source_type);

  FRIEND_TEST_ALL_PREFIXES(PassiveLogCollectorTest,
                           HoldReferenceToDependentSource);
  FRIEND_TEST_ALL_PREFIXES(PassiveLogCollectorTest,
                           HoldReferenceToDeletedSource);

  GlobalSourceTracker global_source_tracker_;
  ConnectJobTracker connect_job_tracker_;
  SocketTracker socket_tracker_;
  RequestTracker url_request_tracker_;
  RequestTracker socket_stream_tracker_;
  InitProxyResolverTracker init_proxy_resolver_tracker_;
  SpdySessionTracker spdy_session_tracker_;
  DNSRequestTracker dns_request_tracker_;
  DNSJobTracker dns_job_tracker_;
  DiskCacheEntryTracker disk_cache_entry_tracker_;
  MemCacheEntryTracker mem_cache_entry_tracker_;
  HttpStreamJobTracker http_stream_job_tracker_;

  // This array maps each NetLog::SourceType to one of the tracker instances
  // defined above. Use of this array avoid duplicating the list of trackers
  // elsewhere.
  SourceTrackerInterface* trackers_[net::NetLog::SOURCE_COUNT];

  // The count of how many events have flowed through this log. Used to set the
  // "order" field on captured events.
  uint32 num_events_seen_;

  DISALLOW_COPY_AND_ASSIGN(PassiveLogCollector);
};

#endif  // CHROME_BROWSER_NET_PASSIVE_LOG_COLLECTOR_H_
