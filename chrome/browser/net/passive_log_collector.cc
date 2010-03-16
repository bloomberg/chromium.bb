// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/passive_log_collector.h"

#include <algorithm>

#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"

namespace {
const size_t kMaxNumEntriesPerLog = 50;
const size_t kMaxConnectJobGraveyardSize = 3;
const size_t kMaxRequestGraveyardSize = 25;
const size_t kMaxLiveRequests = 200;

// Sort function on source ID.
bool OrderBySourceID(const PassiveLogCollector::RequestInfo& a,
                     const PassiveLogCollector::RequestInfo& b) {
  return a.entries[0].source.id < b.entries[0].source.id;
}

void AddEntryToRequestInfo(const net::NetLog::Entry& entry,
                           bool is_unbounded,
                           PassiveLogCollector::RequestInfo* out_info) {
  // Start dropping new entries when the log has gotten too big.
  if (out_info->entries.size() + 1 <= kMaxNumEntriesPerLog || is_unbounded) {
    out_info->entries.push_back(entry);
  } else {
    out_info->num_entries_truncated += 1;
    out_info->entries[kMaxNumEntriesPerLog - 1] = entry;
  }
}

void AppendToRequestInfo(const PassiveLogCollector::RequestInfo& info,
                         bool is_unbounded,
                         PassiveLogCollector::RequestInfo* out_info) {
  for (size_t i = 0; i < info.entries.size(); ++i)
    AddEntryToRequestInfo(info.entries[i], is_unbounded, out_info);
}

}  // namespace

//----------------------------------------------------------------------------
// PassiveLogCollector
//----------------------------------------------------------------------------

PassiveLogCollector::PassiveLogCollector()
    : url_request_tracker_(&connect_job_tracker_),
      socket_stream_tracker_(&connect_job_tracker_) {
}

PassiveLogCollector::~PassiveLogCollector() {
}

void PassiveLogCollector::OnAddEntry(const net::NetLog::Entry& entry) {
  switch (entry.source.type) {
    case net::NetLog::SOURCE_URL_REQUEST:
      url_request_tracker_.OnAddEntry(entry);
      break;
    case net::NetLog::SOURCE_SOCKET_STREAM:
      socket_stream_tracker_.OnAddEntry(entry);
      break;
    case net::NetLog::SOURCE_CONNECT_JOB:
      connect_job_tracker_.OnAddEntry(entry);
      break;
    case net::NetLog::SOURCE_INIT_PROXY_RESOLVER:
      init_proxy_resolver_tracker_.OnAddEntry(entry);
      break;
    default:
      // Drop all other logged events.
      break;
  }
}

void PassiveLogCollector::Clear() {
  connect_job_tracker_.Clear();
  url_request_tracker_.Clear();
  socket_stream_tracker_.Clear();
}

//----------------------------------------------------------------------------
// RequestTrackerBase
//----------------------------------------------------------------------------

PassiveLogCollector::RequestTrackerBase::RequestTrackerBase(
    size_t max_graveyard_size)
    : max_graveyard_size_(max_graveyard_size),
      next_graveyard_index_(0),
      is_unbounded_(false) {
}

void PassiveLogCollector::RequestTrackerBase::OnAddEntry(
    const net::NetLog::Entry& entry) {
  RequestInfo& info = live_requests_[entry.source.id];
  Action result = DoAddEntry(entry, &info);

  switch (result) {
    case ACTION_MOVE_TO_GRAVEYARD:
      InsertIntoGraveyard(info);
      // (fall-through)
    case ACTION_DELETE:
      RemoveFromLiveRequests(entry.source.id);
      break;
    default:
      break;
  }

  if (live_requests_.size() > kMaxLiveRequests) {
    // This is a safety net in case something went wrong, to avoid continually
    // growing memory.
    LOG(WARNING) << "The passive log data has grown larger "
                    "than expected, resetting";
    live_requests_.clear();
  }
}

PassiveLogCollector::RequestInfoList
PassiveLogCollector::RequestTrackerBase::GetLiveRequests() const {
  RequestInfoList list;

  // Copy all of the live requests into the vector.
  for (SourceIDToInfoMap::const_iterator it = live_requests_.begin();
       it != live_requests_.end();
       ++it) {
    list.push_back(it->second);
  }

  std::sort(list.begin(), list.end(), OrderBySourceID);
  return list;
}

void PassiveLogCollector::RequestTrackerBase::ClearRecentlyDeceased() {
  next_graveyard_index_ = 0;
  graveyard_.clear();
}

// Returns a list of recently completed Requests.
PassiveLogCollector::RequestInfoList
PassiveLogCollector::RequestTrackerBase::GetRecentlyDeceased() const {
  RequestInfoList list;

  // Copy the items from |graveyard_| (our circular queue of recently
  // deceased request infos) into a vector, ordered from oldest to newest.
  for (size_t i = 0; i < graveyard_.size(); ++i) {
    size_t index = (next_graveyard_index_ + i) % graveyard_.size();
    list.push_back(graveyard_[index]);
  }
  return list;
}

const PassiveLogCollector::RequestInfo*
PassiveLogCollector::RequestTrackerBase::GetRequestInfoFromGraveyard(
    int source_id) const {
  // Scan through the graveyard to find an entry for |source_id|.
  for (size_t i = 0; i < graveyard_.size(); ++i) {
    if (graveyard_[i].entries[0].source.id == source_id) {
      return &graveyard_[i];
    }
  }
  return NULL;
}

void PassiveLogCollector::RequestTrackerBase::RemoveFromLiveRequests(
    int source_id) {
  // Remove from |live_requests_|.
  SourceIDToInfoMap::iterator it = live_requests_.find(source_id);
  // TODO(eroman): Shouldn't have this 'if', is it actually really necessary?
  if (it != live_requests_.end())
    live_requests_.erase(it);
}

void PassiveLogCollector::RequestTrackerBase::SetUnbounded(
    bool unbounded) {
  // No change.
  if (is_unbounded_ == unbounded)
    return;

  // If we are going from unbounded to bounded, we need to trim the
  // graveyard. For simplicity we will simply clear it.
  if (is_unbounded_ && !unbounded)
    ClearRecentlyDeceased();

  is_unbounded_ = unbounded;
}

void PassiveLogCollector::RequestTrackerBase::Clear() {
  ClearRecentlyDeceased();
  live_requests_.clear();
}

void PassiveLogCollector::RequestTrackerBase::InsertIntoGraveyard(
    const RequestInfo& info) {
  if (is_unbounded_) {
    graveyard_.push_back(info);
    return;
  }

  // Otherwise enforce a bound on the graveyard size, by treating it as a
  // circular buffer.
  if (graveyard_.size() < max_graveyard_size_) {
    // Still growing to maximum capacity.
    DCHECK_EQ(next_graveyard_index_, graveyard_.size());
    graveyard_.push_back(info);
  } else {
    // At maximum capacity, overwite the oldest entry.
    graveyard_[next_graveyard_index_] = info;
  }
  next_graveyard_index_ = (next_graveyard_index_ + 1) % max_graveyard_size_;
}

//----------------------------------------------------------------------------
// ConnectJobTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::ConnectJobTracker::kMaxGraveyardSize = 3;

PassiveLogCollector::ConnectJobTracker::ConnectJobTracker()
    : RequestTrackerBase(kMaxGraveyardSize) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::ConnectJobTracker::DoAddEntry(
    const net::NetLog::Entry& entry,
    RequestInfo* out_info) {
  // Save the entry (possibly truncating).
  AddEntryToRequestInfo(entry, is_unbounded(), out_info);

  // If this is the end of the connect job, move the request to the graveyard.
  if (entry.type == net::NetLog::Entry::TYPE_EVENT &&
      entry.event.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB &&
      entry.event.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

//----------------------------------------------------------------------------
// RequestTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::RequestTracker::kMaxGraveyardSize = 25;
const size_t PassiveLogCollector::RequestTracker::kMaxGraveyardURLSize = 1000;

PassiveLogCollector::RequestTracker::RequestTracker(
    ConnectJobTracker* connect_job_tracker)
    : RequestTrackerBase(kMaxGraveyardSize),
      connect_job_tracker_(connect_job_tracker) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::RequestTracker::DoAddEntry(
    const net::NetLog::Entry& entry,
    RequestInfo* out_info) {

  if (entry.type == net::NetLog::Entry::TYPE_EVENT &&
      entry.event.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID) {
    // If this was notification that a ConnectJob was bound to the request,
    // copy all the logged data for that ConnectJob.
    AddConnectJobInfo(entry, out_info);
  } else {
    // Otherwise just append this entry to the request info.
    AddEntryToRequestInfo(entry, is_unbounded(), out_info);
  }

  // If this was the start of a URLRequest/SocketStream, extract the URL.
  if (out_info->entries.size() == 1 &&
      entry.type == net::NetLog::Entry::TYPE_EVENT &&
      entry.event.type == net::NetLog::TYPE_REQUEST_ALIVE &&
      entry.event.phase == net::NetLog::PHASE_BEGIN) {
    out_info->url = entry.string;
    out_info->entries[0].string = std::string();

     // Paranoia check: truncate the URL if it is really big.
    if (out_info->url.size() > kMaxGraveyardURLSize)
      out_info->url = out_info->url.substr(0, kMaxGraveyardURLSize);
  }

  // If the request has ended, move it to the graveyard.
  if (entry.type == net::NetLog::Entry::TYPE_EVENT &&
      entry.event.type == net::NetLog::TYPE_REQUEST_ALIVE &&
      entry.event.phase == net::NetLog::PHASE_END) {
    if (StartsWithASCII(out_info->url, "chrome://", false)) {
      // Avoid sending "chrome://" requests to the graveyard, since it just
      // adds to clutter.
      return ACTION_DELETE;
    }
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

void PassiveLogCollector::RequestTracker::AddConnectJobInfo(
    const net::NetLog::Entry& entry,
    RequestInfo* live_entry) {
  // We have just been notified of which ConnectJob the
  // URLRequest/SocketStream was assigned. Lookup all the data we captured
  // for the ConnectJob, and append it to the URLRequest/SocketStream's
  // RequestInfo.

  // TODO(eroman): This should NOT be plumbed through via |error_code| !
  int connect_job_id = entry.error_code;

  const RequestInfo* connect_job_info =
      connect_job_tracker_->GetRequestInfoFromGraveyard(connect_job_id);

  if (connect_job_info) {
    // Append the ConnectJob information we found.
    AppendToRequestInfo(*connect_job_info, is_unbounded(), live_entry);
  } else {
    // If we couldn't find the information for the ConnectJob, append a
    // generic message instead.
    net::NetLog::Entry e(entry);
    e.type = net::NetLog::Entry::TYPE_STRING;
    e.string = StringPrintf("Used ConnectJob id=%d", connect_job_id);
    AddEntryToRequestInfo(e, is_unbounded(), live_entry);
  }
}

//----------------------------------------------------------------------------
// InitProxyResolverTracker
//----------------------------------------------------------------------------

PassiveLogCollector::InitProxyResolverTracker::InitProxyResolverTracker() {}

void PassiveLogCollector::InitProxyResolverTracker::OnAddEntry(
    const net::NetLog::Entry& entry) {
  if (entry.type == net::NetLog::Entry::TYPE_EVENT &&
      entry.event.type == net::NetLog::TYPE_INIT_PROXY_RESOLVER &&
      entry.event.phase == net::NetLog::PHASE_BEGIN) {
    // If this is the start of a new InitProxyResolver, overwrite the old data.
    entries_.clear();
    entries_.push_back(entry);
  } else {
    // Otherwise append it to the log for the latest InitProxyResolver.
    if (!entries_.empty() && entries_[0].source.id != entry.source.id) {
      // If this entry doesn't match what we think was the latest
      // InitProxyResolver, drop it. (This shouldn't happen, but we will guard
      // against it).
      return;
    }
    entries_.push_back(entry);
  }

  // Safety net: INIT_PROXY_RESOLVER shouldn't generate many messages, but in
  // case something goes wrong, avoid exploding the memory usage.
  if (entries_.size() > kMaxNumEntriesPerLog)
    entries_.clear();
}

