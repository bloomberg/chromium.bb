// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/passive_log_collector.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/format_macros.h"
#include "chrome/browser/chrome_thread.h"
#include "net/url_request/url_request_netlog_params.h"

namespace {

// TODO(eroman): Do something with the truncation count.

const size_t kMaxNumEntriesPerLog = 30;
const size_t kMaxRequestsPerTracker = 200;

void AddEntryToRequestInfo(const PassiveLogCollector::Entry& entry,
                           PassiveLogCollector::RequestInfo* out_info) {
  // Start dropping new entries when the log has gotten too big.
  if (out_info->entries.size() + 1 <= kMaxNumEntriesPerLog) {
    out_info->entries.push_back(entry);
  } else {
    out_info->num_entries_truncated += 1;
    out_info->entries[kMaxNumEntriesPerLog - 1] = entry;
  }
}

// Comparator to sort entries by their |order| property, ascending.
bool SortByOrderComparator(const PassiveLogCollector::Entry& a,
                           const PassiveLogCollector::Entry& b) {
  return a.order < b.order;
}

}  // namespace

//----------------------------------------------------------------------------
// PassiveLogCollector
//----------------------------------------------------------------------------

PassiveLogCollector::PassiveLogCollector()
    : ALLOW_THIS_IN_INITIALIZER_LIST(connect_job_tracker_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(url_request_tracker_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(socket_stream_tracker_(this)),
      num_events_seen_(0) {

  // Define the mapping between source types and the tracker objects.
  memset(&trackers_[0], 0, sizeof(trackers_));
  trackers_[net::NetLog::SOURCE_URL_REQUEST] = &url_request_tracker_;
  trackers_[net::NetLog::SOURCE_SOCKET_STREAM] = &socket_stream_tracker_;
  trackers_[net::NetLog::SOURCE_CONNECT_JOB] = &connect_job_tracker_;
  trackers_[net::NetLog::SOURCE_SOCKET] = &socket_tracker_;
  trackers_[net::NetLog::SOURCE_INIT_PROXY_RESOLVER] =
      &init_proxy_resolver_tracker_;
  trackers_[net::NetLog::SOURCE_SPDY_SESSION] = &spdy_session_tracker_;

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
  // Package the parameters into a single struct for convenience.
  Entry entry(num_events_seen_++, type, time, source, phase, params);

  RequestTrackerBase* tracker = GetTrackerForSourceType(entry.source.type);
  if (tracker)
    tracker->OnAddEntry(entry);
}

PassiveLogCollector::RequestTrackerBase*
PassiveLogCollector::GetTrackerForSourceType(
    net::NetLog::SourceType source_type) {
  DCHECK_LE(source_type, static_cast<int>(arraysize(trackers_)));
  DCHECK_GE(source_type, 0);
  return trackers_[source_type];
}

void PassiveLogCollector::Clear() {
  for (size_t i = 0; i < arraysize(trackers_); ++i)
    trackers_[i]->Clear();
}

void PassiveLogCollector::GetAllCapturedEvents(EntryList* out) const {
  out->clear();

  // Append all of the captured entries held by the various trackers to
  // |out|.
  for (size_t i = 0; i < arraysize(trackers_); ++i)
    trackers_[i]->AppendAllEntries(out);

  // Now sort the list of entries by their insertion time (ascending).
  std::sort(out->begin(), out->end(), &SortByOrderComparator);
}

std::string PassiveLogCollector::RequestInfo::GetURL() const {
  // Note: we look at the first *two* entries, since the outer REQUEST_ALIVE
  // doesn't actually contain any data.
  for (size_t i = 0; i < 2 && i < entries.size(); ++i) {
    const PassiveLogCollector::Entry& entry = entries[i];
    if (entry.phase == net::NetLog::PHASE_BEGIN && entry.params) {
      switch (entry.type) {
        case net::NetLog::TYPE_URL_REQUEST_START_JOB:
          return static_cast<URLRequestStartEventParameters*>(
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
// RequestTrackerBase
//----------------------------------------------------------------------------

PassiveLogCollector::RequestTrackerBase::RequestTrackerBase(
    size_t max_graveyard_size, PassiveLogCollector* parent)
    : max_graveyard_size_(max_graveyard_size), parent_(parent) {
}

PassiveLogCollector::RequestTrackerBase::~RequestTrackerBase() {}

void PassiveLogCollector::RequestTrackerBase::OnAddEntry(const Entry& entry) {
  RequestInfo& info = requests_[entry.source.id];
  info.source_id = entry.source.id;  // In case this is a new entry.
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
          DeleteRequestInfo(info.source_id);
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }

  if (requests_.size() > kMaxRequestsPerTracker) {
    // This is a safety net in case something went wrong, to avoid continually
    // growing memory.
    LOG(WARNING) << "The passive log data has grown larger "
                    "than expected, resetting";
    Clear();
  }
}

void PassiveLogCollector::RequestTrackerBase::DeleteRequestInfo(
    uint32 source_id) {
  SourceIDToInfoMap::iterator it = requests_.find(source_id);
  DCHECK(it != requests_.end());
  // The request should not be in the deletion queue.
  DCHECK(std::find(deletion_queue_.begin(), deletion_queue_.end(),
                   source_id) == deletion_queue_.end());
  ReleaseAllReferencesToDependencies(&(it->second));
  requests_.erase(it);
}

void PassiveLogCollector::RequestTrackerBase::Clear() {
  deletion_queue_.clear();

  // Release all references held to dependent sources.
  for (SourceIDToInfoMap::iterator it = requests_.begin();
       it != requests_.end();
       ++it) {
    ReleaseAllReferencesToDependencies(&(it->second));
  }
  requests_.clear();
}

void PassiveLogCollector::RequestTrackerBase::AppendAllEntries(
    EntryList* out) const {
  // Append all of the entries for each of the sources.
  for (SourceIDToInfoMap::const_iterator it = requests_.begin();
       it != requests_.end();
       ++it) {
    const RequestInfo& info = it->second;
    out->insert(out->end(), info.entries.begin(), info.entries.end());
  }
}

void PassiveLogCollector::RequestTrackerBase::AddToDeletionQueue(
    uint32 source_id) {
  DCHECK(requests_.find(source_id) != requests_.end());
  DCHECK(!requests_.find(source_id)->second.is_alive);
  DCHECK_GE(requests_.find(source_id)->second.reference_count, 0);
  DCHECK_LE(deletion_queue_.size(), max_graveyard_size_);

  deletion_queue_.push_back(source_id);

  // After the deletion queue has reached its maximum size, start
  // deleting requests in FIFO order.
  if (deletion_queue_.size() > max_graveyard_size_) {
    uint32 oldest = deletion_queue_.front();
    deletion_queue_.pop_front();
    DeleteRequestInfo(oldest);
  }
}

void PassiveLogCollector::RequestTrackerBase::AdjustReferenceCountForSource(
    int offset, uint32 source_id) {
  DCHECK(offset == -1 || offset == 1) << "invalid offset: " << offset;

  // In general it is invalid to call AdjustReferenceCountForSource() on
  // source that doesn't exist. However, it is possible that if
  // RequestTrackerBase::Clear() was previously called this can happen.
  // TODO(eroman): Add a unit-test that exercises this case.
  SourceIDToInfoMap::iterator it = requests_.find(source_id);
  if (it == requests_.end())
    return;

  RequestInfo& info = it->second;
  DCHECK_GE(info.reference_count, 0);
  DCHECK_GE(info.reference_count + offset, 0);
  info.reference_count += offset;

  if (!info.is_alive) {
    if (info.reference_count == 1 && offset == 1) {
      // If we just added a reference to a dead source that had no references,
      // it must have been in the deletion queue, so remove it from the queue.
      DeletionQueue::iterator it =
          std::remove(deletion_queue_.begin(), deletion_queue_.end(),
                      source_id);
      DCHECK(it != deletion_queue_.end());
      deletion_queue_.erase(it);
    } else if (info.reference_count == 0) {
      // If we just released the final reference to a dead request, go ahead
      // and delete it right away.
      DeleteRequestInfo(source_id);
    }
  }
}

void PassiveLogCollector::RequestTrackerBase::AddReferenceToSourceDependency(
    const net::NetLog::Source& source, RequestInfo* info) {
  // Find the tracker which should be holding |source|.
  DCHECK(parent_);
  RequestTrackerBase* tracker =
      parent_->GetTrackerForSourceType(source.type);
  DCHECK(tracker);

  // Tell the owning tracker to increment the reference count of |source|.
  tracker->AdjustReferenceCountForSource(1, source.id);

  // Make a note to release this reference once |info| is destroyed.
  info->dependencies.push_back(source);
}

void
PassiveLogCollector::RequestTrackerBase::ReleaseAllReferencesToDependencies(
    RequestInfo* info) {
  // Release all references |info| was holding to dependent sources.
  for (SourceDependencyList::const_iterator it = info->dependencies.begin();
       it != info->dependencies.end(); ++it) {
    const net::NetLog::Source& source = *it;

    // Find the tracker which should be holding |source|.
    DCHECK(parent_);
    RequestTrackerBase* tracker =
        parent_->GetTrackerForSourceType(source.type);
    DCHECK(tracker);

    // Tell the owning tracker to decrement the reference count of |source|.
    tracker->AdjustReferenceCountForSource(-1, source.id);
  }

  info->dependencies.clear();
}

//----------------------------------------------------------------------------
// ConnectJobTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::ConnectJobTracker::kMaxGraveyardSize = 15;

PassiveLogCollector::ConnectJobTracker::ConnectJobTracker(
    PassiveLogCollector* parent)
    : RequestTrackerBase(kMaxGraveyardSize, parent) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::ConnectJobTracker::DoAddEntry(const Entry& entry,
                                                   RequestInfo* out_info) {
  AddEntryToRequestInfo(entry, out_info);

  if (entry.type == net::NetLog::TYPE_CONNECT_JOB_SET_SOCKET) {
    const net::NetLog::Source& source_dependency =
        static_cast<net::NetLogSourceParameter*>(entry.params.get())->value();
    AddReferenceToSourceDependency(source_dependency, out_info);
  }

  // If this is the end of the connect job, move the request to the graveyard.
  if (entry.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// SocketTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::SocketTracker::kMaxGraveyardSize = 15;

PassiveLogCollector::SocketTracker::SocketTracker()
    : RequestTrackerBase(kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::SocketTracker::DoAddEntry(const Entry& entry,
                                               RequestInfo* out_info) {
  // TODO(eroman): aggregate the byte counts once truncation starts to happen,
  //               to summarize transaction read/writes for each SOCKET_IN_USE
  //               section.
  if (entry.type == net::NetLog::TYPE_SOCKET_BYTES_SENT ||
      entry.type == net::NetLog::TYPE_SOCKET_BYTES_RECEIVED) {
    return ACTION_NONE;
  }

  AddEntryToRequestInfo(entry, out_info);

  if (entry.type == net::NetLog::TYPE_SOCKET_ALIVE &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// RequestTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::RequestTracker::kMaxGraveyardSize = 25;

PassiveLogCollector::RequestTracker::RequestTracker(PassiveLogCollector* parent)
    : RequestTrackerBase(kMaxGraveyardSize, parent) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::RequestTracker::DoAddEntry(const Entry& entry,
                                                RequestInfo* out_info) {
  if (entry.type == net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB ||
      entry.type == net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET) {
    const net::NetLog::Source& source_dependency =
        static_cast<net::NetLogSourceParameter*>(entry.params.get())->value();
    AddReferenceToSourceDependency(source_dependency, out_info);
  }

  AddEntryToRequestInfo(entry, out_info);

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

const size_t PassiveLogCollector::InitProxyResolverTracker::kMaxGraveyardSize =
    3;

PassiveLogCollector::InitProxyResolverTracker::InitProxyResolverTracker()
    : RequestTrackerBase(kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::InitProxyResolverTracker::DoAddEntry(
    const Entry& entry, RequestInfo* out_info) {
  AddEntryToRequestInfo(entry, out_info);
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

const size_t PassiveLogCollector::SpdySessionTracker::kMaxGraveyardSize = 10;

PassiveLogCollector::SpdySessionTracker::SpdySessionTracker()
    : RequestTrackerBase(kMaxGraveyardSize, NULL) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::SpdySessionTracker::DoAddEntry(const Entry& entry,
                                                    RequestInfo* out_info) {
  AddEntryToRequestInfo(entry, out_info);
  if (entry.type == net::NetLog::TYPE_SPDY_SESSION &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  } else {
    return ACTION_NONE;
  }
}
