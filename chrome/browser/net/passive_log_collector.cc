// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/passive_log_collector.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/format_macros.h"
#include "net/url_request/url_request_netlog_params.h"

namespace {

// TODO(eroman): Do something with the truncation count.

const size_t kMaxNumEntriesPerLog = 30;

void AddEntryToSourceInfo(const ChromeNetLog::Entry& entry,
                          PassiveLogCollector::SourceInfo* out_info) {
  // Start dropping new entries when the log has gotten too big.
  if (out_info->entries.size() + 1 <= kMaxNumEntriesPerLog) {
    out_info->entries.push_back(entry);
  } else {
    out_info->num_entries_truncated += 1;
    out_info->entries[kMaxNumEntriesPerLog - 1] = entry;
  }
}

// Comparator to sort entries by their |order| property, ascending.
bool SortByOrderComparator(const ChromeNetLog::Entry& a,
                           const ChromeNetLog::Entry& b) {
  return a.order < b.order;
}

}  // namespace

PassiveLogCollector::SourceInfo::SourceInfo()
    : source_id(net::NetLog::Source::kInvalidId),
      num_entries_truncated(0),
      reference_count(0),
      is_alive(true) {
}

PassiveLogCollector::SourceInfo::~SourceInfo() {}

//----------------------------------------------------------------------------
// PassiveLogCollector
//----------------------------------------------------------------------------

PassiveLogCollector::PassiveLogCollector()
    : ThreadSafeObserver(net::NetLog::LOG_BASIC),
      ALLOW_THIS_IN_INITIALIZER_LIST(connect_job_tracker_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(url_request_tracker_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(socket_stream_tracker_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(http_stream_job_tracker_(this)),
      num_events_seen_(0) {

  // Define the mapping between source types and the tracker objects.
  memset(&trackers_[0], 0, sizeof(trackers_));
  trackers_[net::NetLog::SOURCE_NONE] = &global_source_tracker_;
  trackers_[net::NetLog::SOURCE_URL_REQUEST] = &url_request_tracker_;
  trackers_[net::NetLog::SOURCE_SOCKET_STREAM] = &socket_stream_tracker_;
  trackers_[net::NetLog::SOURCE_CONNECT_JOB] = &connect_job_tracker_;
  trackers_[net::NetLog::SOURCE_SOCKET] = &socket_tracker_;
  trackers_[net::NetLog::SOURCE_INIT_PROXY_RESOLVER] =
      &init_proxy_resolver_tracker_;
  trackers_[net::NetLog::SOURCE_SPDY_SESSION] = &spdy_session_tracker_;
  trackers_[net::NetLog::SOURCE_HOST_RESOLVER_IMPL_REQUEST] =
      &dns_request_tracker_;
  trackers_[net::NetLog::SOURCE_HOST_RESOLVER_IMPL_JOB] = &dns_job_tracker_;
  trackers_[net::NetLog::SOURCE_DISK_CACHE_ENTRY] = &disk_cache_entry_tracker_;
  trackers_[net::NetLog::SOURCE_MEMORY_CACHE_ENTRY] = &mem_cache_entry_tracker_;
  trackers_[net::NetLog::SOURCE_HTTP_STREAM_JOB] = &http_stream_job_tracker_;
  // Make sure our mapping is up-to-date.
  for (size_t i = 0; i < arraysize(trackers_); ++i)
    DCHECK(trackers_[i]) << "Unhandled SourceType: " << i;
}

PassiveLogCollector::~PassiveLogCollector() {
}

void PassiveLogCollector::OnAddEntry(
    net::NetLog::EventType type,
    const base::TimeTicks& time,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    net::NetLog::EventParameters* params) {
  AssertNetLogLockAcquired();
  // Package the parameters into a single struct for convenience.
  ChromeNetLog::Entry entry(num_events_seen_++, type, time, source, phase,
                            params);

  SourceTrackerInterface* tracker = GetTrackerForSourceType(entry.source.type);
  if (tracker)
    tracker->OnAddEntry(entry);
}

void PassiveLogCollector::Clear() {
  AssertNetLogLockAcquired();
  for (size_t i = 0; i < arraysize(trackers_); ++i)
    trackers_[i]->Clear();
}

PassiveLogCollector::SourceTrackerInterface*
PassiveLogCollector::GetTrackerForSourceType(
    net::NetLog::SourceType source_type) {
  CHECK_LT(source_type, static_cast<int>(arraysize(trackers_)));
  CHECK_GE(source_type, 0);
  return trackers_[source_type];
}

void PassiveLogCollector::GetAllCapturedEvents(
    ChromeNetLog::EntryList* out) const {
  AssertNetLogLockAcquired();
  out->clear();

  // Append all of the captured entries held by the various trackers to
  // |out|.
  for (size_t i = 0; i < arraysize(trackers_); ++i)
    trackers_[i]->AppendAllEntries(out);

  // Now sort the list of entries by their insertion time (ascending).
  std::sort(out->begin(), out->end(), &SortByOrderComparator);
}

std::string PassiveLogCollector::SourceInfo::GetURL() const {
  // Note: we look at the first *two* entries, since the outer REQUEST_ALIVE
  // doesn't actually contain any data.
  for (size_t i = 0; i < 2 && i < entries.size(); ++i) {
    const ChromeNetLog::Entry& entry = entries[i];
    if (entry.phase == net::NetLog::PHASE_BEGIN && entry.params) {
      switch (entry.type) {
        case net::NetLog::TYPE_URL_REQUEST_START_JOB:
          return static_cast<net::URLRequestStartEventParameters*>(
              entry.params.get())->url().possibly_invalid_spec();
        case net::NetLog::TYPE_SOCKET_STREAM_CONNECT:
          return static_cast<net::NetLogStringParameter*>(
              entry.params.get())->value();
        default:
          break;
      }
    }
  }
  return std::string();
}

//----------------------------------------------------------------------------
// GlobalSourceTracker
//----------------------------------------------------------------------------

PassiveLogCollector::GlobalSourceTracker::GlobalSourceTracker() {}
PassiveLogCollector::GlobalSourceTracker::~GlobalSourceTracker() {}

void PassiveLogCollector::GlobalSourceTracker::OnAddEntry(
    const ChromeNetLog::Entry& entry) {
  const size_t kMaxEntries = 30u;
  entries_.push_back(entry);
  if (entries_.size() > kMaxEntries)
    entries_.pop_front();
}

void PassiveLogCollector::GlobalSourceTracker::Clear() {
  entries_.clear();
}

void PassiveLogCollector::GlobalSourceTracker::AppendAllEntries(
    ChromeNetLog::EntryList* out) const {
  out->insert(out->end(), entries_.begin(), entries_.end());
}

//----------------------------------------------------------------------------
// SourceTracker
//----------------------------------------------------------------------------

PassiveLogCollector::SourceTracker::SourceTracker(
    size_t max_num_sources,
    size_t max_graveyard_size,
    PassiveLogCollector* parent)
    : max_num_sources_(max_num_sources),
      max_graveyard_size_(max_graveyard_size),
      parent_(parent) {
}

PassiveLogCollector::SourceTracker::~SourceTracker() {}

void PassiveLogCollector::SourceTracker::OnAddEntry(
    const ChromeNetLog::Entry& entry) {
  // Lookup or insert a new entry into the bounded map.
  SourceIDToInfoMap::iterator it = sources_.find(entry.source.id);
  if (it == sources_.end()) {
    if (sources_.size() >= max_num_sources_) {
      LOG(WARNING) << "The passive log data has grown larger "
                      "than expected, resetting";
      Clear();
    }
    it = sources_.insert(
        SourceIDToInfoMap::value_type(entry.source.id, SourceInfo())).first;
    it->second.source_id = entry.source.id;
  }

  SourceInfo& info = it->second;
  Action result = DoAddEntry(entry, &info);

  if (result != ACTION_NONE) {
    // We are either queuing it for deletion, or deleting it immediately.
    // If someone else holds a reference to this source, defer the deletion
    // until all the references are released.
    info.is_alive = false;
    if (info.reference_count == 0) {
      switch (result) {
        case ACTION_MOVE_TO_GRAVEYARD:
          AddToDeletionQueue(info.source_id);
          break;
        case ACTION_DELETE:
          DeleteSourceInfo(info.source_id);
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }
}

void PassiveLogCollector::SourceTracker::DeleteSourceInfo(
    uint32 source_id) {
  SourceIDToInfoMap::iterator it = sources_.find(source_id);
  if (it == sources_.end()) {
    // TODO(eroman): Is this happening? And if so, why. Remove this
    //               once the cause is understood.
    LOG(WARNING) << "Tried to delete info for nonexistent source";
    return;
  }
  // The source should not be in the deletion queue.
  CHECK(std::find(deletion_queue_.begin(), deletion_queue_.end(),
                  source_id) == deletion_queue_.end());
  ReleaseAllReferencesToDependencies(&(it->second));
  sources_.erase(it);
}

void PassiveLogCollector::SourceTracker::Clear() {
  deletion_queue_.clear();

  // Release all references held to dependent sources.
  for (SourceIDToInfoMap::iterator it = sources_.begin();
       it != sources_.end();
       ++it) {
    ReleaseAllReferencesToDependencies(&(it->second));
  }
  sources_.clear();
}

void PassiveLogCollector::SourceTracker::AppendAllEntries(
    ChromeNetLog::EntryList* out) const {
  // Append all of the entries for each of the sources.
  for (SourceIDToInfoMap::const_iterator it = sources_.begin();
       it != sources_.end();
       ++it) {
    const SourceInfo& info = it->second;
    out->insert(out->end(), info.entries.begin(), info.entries.end());
  }
}

void PassiveLogCollector::SourceTracker::AddToDeletionQueue(
    uint32 source_id) {
  DCHECK(sources_.find(source_id) != sources_.end());
  DCHECK(!sources_.find(source_id)->second.is_alive);
  DCHECK_GE(sources_.find(source_id)->second.reference_count, 0);
  DCHECK_LE(deletion_queue_.size(), max_graveyard_size_);

  DCHECK(std::find(deletion_queue_.begin(), deletion_queue_.end(),
                   source_id) == deletion_queue_.end());
  deletion_queue_.push_back(source_id);

  // After the deletion queue has reached its maximum size, start
  // deleting sources in FIFO order.
  if (deletion_queue_.size() > max_graveyard_size_) {
    uint32 oldest = deletion_queue_.front();
    deletion_queue_.pop_front();
    DeleteSourceInfo(oldest);
  }
}

void PassiveLogCollector::SourceTracker::EraseFromDeletionQueue(
    uint32 source_id) {
  DeletionQueue::iterator it =
      std::remove(deletion_queue_.begin(), deletion_queue_.end(),
                  source_id);
  CHECK(it != deletion_queue_.end());
  deletion_queue_.erase(it);
}

void PassiveLogCollector::SourceTracker::AdjustReferenceCountForSource(
    int offset, uint32 source_id) {
  DCHECK(offset == -1 || offset == 1) << "invalid offset: " << offset;

  // In general it is invalid to call AdjustReferenceCountForSource() on
  // source that doesn't exist. However, it is possible that if
  // SourceTracker::Clear() was previously called this can happen.
  SourceIDToInfoMap::iterator it = sources_.find(source_id);
  if (it == sources_.end()) {
    LOG(WARNING) << "Released a reference to nonexistent source.";
    return;
  }

  SourceInfo& info = it->second;
  DCHECK_GE(info.reference_count, 0);
  info.reference_count += offset;

  bool released_unmatched_reference = info.reference_count < 0;
  if (released_unmatched_reference) {
    // In general this shouldn't happen, however it is possible to reach this
    // state if SourceTracker::Clear() was called earlier.
    LOG(WARNING) << "Released unmatched reference count.";
    info.reference_count = 0;
  }

  if (!info.is_alive) {
    if (info.reference_count == 1 && offset == 1) {
      // If we just added a reference to a dead source that had no references,
      // it must have been in the deletion queue, so remove it from the queue.
      EraseFromDeletionQueue(source_id);
    } else if (info.reference_count == 0) {
      if (released_unmatched_reference)
        EraseFromDeletionQueue(source_id);
      // If we just released the final reference to a dead source, go ahead
      // and delete it right away.
      DeleteSourceInfo(source_id);
    }
  }
}

void PassiveLogCollector::SourceTracker::AddReferenceToSourceDependency(
    const net::NetLog::Source& source, SourceInfo* info) {
  // Find the tracker which should be holding |source|.
  DCHECK(parent_);
  DCHECK_NE(source.type, net::NetLog::SOURCE_NONE);
  SourceTracker* tracker = static_cast<SourceTracker*>(
      parent_->GetTrackerForSourceType(source.type));
  DCHECK(tracker);

  // Tell the owning tracker to increment the reference count of |source|.
  tracker->AdjustReferenceCountForSource(1, source.id);

  // Make a note to release this reference once |info| is destroyed.
  info->dependencies.push_back(source);
}

void PassiveLogCollector::SourceTracker::ReleaseAllReferencesToDependencies(
    SourceInfo* info) {
  // Release all references |info| was holding to other sources.
  for (SourceDependencyList::const_iterator it = info->dependencies.begin();
       it != info->dependencies.end(); ++it) {
    const net::NetLog::Source& source = *it;

    // Find the tracker which should be holding |source|.
    DCHECK(parent_);
    DCHECK_NE(source.type, net::NetLog::SOURCE_NONE);
    SourceTracker* tracker = static_cast<SourceTracker*>(
        parent_->GetTrackerForSourceType(source.type));
    DCHECK(tracker);

    // Tell the owning tracker to decrement the reference count of |source|.
    tracker->AdjustReferenceCountForSource(-1, source.id);
  }

  info->dependencies.clear();
}

//----------------------------------------------------------------------------
// ConnectJobTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::ConnectJobTracker::kMaxNumSources = 100;
const size_t PassiveLogCollector::ConnectJobTracker::kMaxGraveyardSize = 15;

PassiveLogCollector::ConnectJobTracker::ConnectJobTracker(
    PassiveLogCollector* parent)
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, parent) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::ConnectJobTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  AddEntryToSourceInfo(entry, out_info);

  if (entry.type == net::NetLog::TYPE_CONNECT_JOB_SET_SOCKET) {
    const net::NetLog::Source& source_dependency =
        static_cast<net::NetLogSourceParameter*>(entry.params.get())->value();
    AddReferenceToSourceDependency(source_dependency, out_info);
  }

  // If this is the end of the connect job, move the source to the graveyard.
  if (entry.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// SocketTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::SocketTracker::kMaxNumSources = 200;
const size_t PassiveLogCollector::SocketTracker::kMaxGraveyardSize = 15;

PassiveLogCollector::SocketTracker::SocketTracker()
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::SocketTracker::DoAddEntry(const ChromeNetLog::Entry& entry,
                                               SourceInfo* out_info) {
  // TODO(eroman): aggregate the byte counts once truncation starts to happen,
  //               to summarize transaction read/writes for each SOCKET_IN_USE
  //               section.
  if (entry.type == net::NetLog::TYPE_SOCKET_BYTES_SENT ||
      entry.type == net::NetLog::TYPE_SOCKET_BYTES_RECEIVED) {
    return ACTION_NONE;
  }

  AddEntryToSourceInfo(entry, out_info);

  if (entry.type == net::NetLog::TYPE_SOCKET_ALIVE &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// RequestTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::RequestTracker::kMaxNumSources = 100;
const size_t PassiveLogCollector::RequestTracker::kMaxGraveyardSize = 25;

PassiveLogCollector::RequestTracker::RequestTracker(PassiveLogCollector* parent)
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, parent) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::RequestTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  if (entry.type == net::NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB) {
    const net::NetLog::Source& source_dependency =
        static_cast<net::NetLogSourceParameter*>(entry.params.get())->value();
    AddReferenceToSourceDependency(source_dependency, out_info);
  }

  AddEntryToSourceInfo(entry, out_info);

  // If the request has ended, move it to the graveyard.
  if (entry.type == net::NetLog::TYPE_REQUEST_ALIVE &&
      entry.phase == net::NetLog::PHASE_END) {
    if (StartsWithASCII(out_info->GetURL(), "chrome://", false)) {
      // Avoid sending "chrome://" requests to the graveyard, since it just
      // adds to clutter.
      return ACTION_DELETE;
    }
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// InitProxyResolverTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::InitProxyResolverTracker::kMaxNumSources = 20;
const size_t PassiveLogCollector::InitProxyResolverTracker::kMaxGraveyardSize =
    3;

PassiveLogCollector::InitProxyResolverTracker::InitProxyResolverTracker()
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::InitProxyResolverTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  AddEntryToSourceInfo(entry, out_info);
  if (entry.type == net::NetLog::TYPE_INIT_PROXY_RESOLVER &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  } else {
    return ACTION_NONE;
  }
}

//----------------------------------------------------------------------------
// SpdySessionTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::SpdySessionTracker::kMaxNumSources = 50;
const size_t PassiveLogCollector::SpdySessionTracker::kMaxGraveyardSize = 10;

PassiveLogCollector::SpdySessionTracker::SpdySessionTracker()
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::SpdySessionTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  AddEntryToSourceInfo(entry, out_info);
  if (entry.type == net::NetLog::TYPE_SPDY_SESSION &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  } else {
    return ACTION_NONE;
  }
}

//----------------------------------------------------------------------------
// DNSRequestTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::DNSRequestTracker::kMaxNumSources = 200;
const size_t PassiveLogCollector::DNSRequestTracker::kMaxGraveyardSize = 20;

PassiveLogCollector::DNSRequestTracker::DNSRequestTracker()
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::DNSRequestTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  AddEntryToSourceInfo(entry, out_info);
  if (entry.type == net::NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  } else {
    return ACTION_NONE;
  }
}

//----------------------------------------------------------------------------
// DNSJobTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::DNSJobTracker::kMaxNumSources = 100;
const size_t PassiveLogCollector::DNSJobTracker::kMaxGraveyardSize = 15;

PassiveLogCollector::DNSJobTracker::DNSJobTracker()
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::DNSJobTracker::DoAddEntry(const ChromeNetLog::Entry& entry,
                                               SourceInfo* out_info) {
  AddEntryToSourceInfo(entry, out_info);
  if (entry.type == net::NetLog::TYPE_HOST_RESOLVER_IMPL_JOB &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  } else {
    return ACTION_NONE;
  }
}

//----------------------------------------------------------------------------
// DiskCacheEntryTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::DiskCacheEntryTracker::kMaxNumSources = 100;
const size_t PassiveLogCollector::DiskCacheEntryTracker::kMaxGraveyardSize = 25;

PassiveLogCollector::DiskCacheEntryTracker::DiskCacheEntryTracker()
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::DiskCacheEntryTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  AddEntryToSourceInfo(entry, out_info);

  // If the request has ended, move it to the graveyard.
  if (entry.type == net::NetLog::TYPE_DISK_CACHE_ENTRY_IMPL &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// MemCacheEntryTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::MemCacheEntryTracker::kMaxNumSources = 100;
const size_t PassiveLogCollector::MemCacheEntryTracker::kMaxGraveyardSize = 25;

PassiveLogCollector::MemCacheEntryTracker::MemCacheEntryTracker()
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::MemCacheEntryTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  AddEntryToSourceInfo(entry, out_info);

  // If the request has ended, move it to the graveyard.
  if (entry.type == net::NetLog::TYPE_DISK_CACHE_MEM_ENTRY_IMPL &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// HttpStreamJobTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::HttpStreamJobTracker::kMaxNumSources = 100;
const size_t PassiveLogCollector::HttpStreamJobTracker::kMaxGraveyardSize = 25;

PassiveLogCollector::HttpStreamJobTracker::HttpStreamJobTracker(
    PassiveLogCollector* parent)
    : SourceTracker(kMaxNumSources, kMaxGraveyardSize, parent) {
}

PassiveLogCollector::SourceTracker::Action
PassiveLogCollector::HttpStreamJobTracker::DoAddEntry(
    const ChromeNetLog::Entry& entry, SourceInfo* out_info) {
  if (entry.type == net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB ||
      entry.type == net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET) {
    const net::NetLog::Source& source_dependency =
        static_cast<net::NetLogSourceParameter*>(entry.params.get())->value();
    AddReferenceToSourceDependency(source_dependency, out_info);
  }

  AddEntryToSourceInfo(entry, out_info);

  // If the request has ended, move it to the graveyard.
  if (entry.type == net::NetLog::TYPE_HTTP_STREAM_JOB &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}
