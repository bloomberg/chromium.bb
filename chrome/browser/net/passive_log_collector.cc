// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/passive_log_collector.h"

#include <algorithm>

#include "base/string_util.h"
#include "base/format_macros.h"
#include "chrome/browser/chrome_thread.h"

namespace {
const size_t kMaxNumEntriesPerLog = 50;
const size_t kMaxConnectJobGraveyardSize = 3;
const size_t kMaxRequestGraveyardSize = 25;
const size_t kMaxLiveRequests = 200;

// Sort function on source ID.
bool OrderBySourceID(const PassiveLogCollector::RequestInfo& a,
                     const PassiveLogCollector::RequestInfo& b) {
  return a.source_id < b.source_id;
}

void AddEntryToRequestInfo(const PassiveLogCollector::Entry& entry,
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

// Appends all of the logged events in |input| to |out|.
void AppendAllEntriesFromRequests(
    const PassiveLogCollector::RequestInfoList& input,
    PassiveLogCollector::EntryList* out) {
  for (size_t i = 0; i < input.size(); ++i) {
    const PassiveLogCollector::EntryList& entries = input[i].entries;
    out->insert(out->end(), entries.begin(), entries.end());
  }
}

// Comparator to sort entries by their |order| property, ascending.
bool SortByOrderComparator(const PassiveLogCollector::Entry& a,
                           const PassiveLogCollector::Entry& b) {
  return a.order < b.order;
}

void SetSubordinateSource(PassiveLogCollector::RequestInfo* info,
                          const PassiveLogCollector::Entry& entry) {
  info->subordinate_source.id = static_cast<net::NetLogIntegerParameter*>(
      entry.params.get())->value();
  switch (entry.type) {
    case net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID:
      info->subordinate_source.type = net::NetLog::SOURCE_CONNECT_JOB;
      break;
    case net::NetLog::TYPE_SOCKET_POOL_SOCKET_ID:
      info->subordinate_source.type = net::NetLog::SOURCE_SOCKET;
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

//----------------------------------------------------------------------------
// PassiveLogCollector
//----------------------------------------------------------------------------

PassiveLogCollector::PassiveLogCollector()
    : url_request_tracker_(&connect_job_tracker_, &socket_tracker_),
      socket_stream_tracker_(&connect_job_tracker_, &socket_tracker_),
      num_events_seen_(0) {
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
    case net::NetLog::SOURCE_SOCKET:
      socket_tracker_.OnAddEntry(entry);
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

void PassiveLogCollector::GetAllCapturedEvents(EntryList* out) const {
  out->clear();

  // Append all of the captured entries held by the various trackers to
  // |out|.
  socket_stream_tracker_.AppendAllEntries(out);
  url_request_tracker_.AppendAllEntries(out);

  const EntryList& proxy_entries =
      init_proxy_resolver_tracker_.entries();
  out->insert(out->end(), proxy_entries.begin(), proxy_entries.end());

  // Now sort the list of entries by their insertion time (ascending).
  std::sort(out->begin(), out->end(), &SortByOrderComparator);
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

void PassiveLogCollector::RequestTrackerBase::OnAddEntry(const Entry& entry) {
  RequestInfo& info = live_requests_[entry.source.id];
  info.source_id = entry.source.id;  // In case this is a new entry.
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
    // We pass the copy (made by the list insert), so changes made in
    // OnLiveRequest are only seen by our caller.
    OnLiveRequest(&list.back());
    std::sort(list.back().entries.begin(), list.back().entries.end(),
              SortByOrderComparator);
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

PassiveLogCollector::RequestInfo*
PassiveLogCollector::RequestTrackerBase::GetRequestInfo(uint32 source_id) {
  // Look for it in the live requests first.
  SourceIDToInfoMap::iterator it = live_requests_.find(source_id);
  if (it != live_requests_.end())
    return &(it->second);

  // Otherwise, scan through the graveyard to find an entry for |source_id|.
  for (size_t i = 0; i < graveyard_.size(); ++i) {
    if (graveyard_[i].source_id == source_id) {
      return &graveyard_[i];
    }
  }
  return NULL;
}

void PassiveLogCollector::RequestTrackerBase::RemoveFromLiveRequests(
    uint32 source_id) {
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

void PassiveLogCollector::RequestTrackerBase::AppendAllEntries(
    EntryList* out) const {
  AppendAllEntriesFromRequests(GetLiveRequests(), out);
  AppendAllEntriesFromRequests(GetRecentlyDeceased(), out);
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

const size_t PassiveLogCollector::ConnectJobTracker::kMaxGraveyardSize = 15;

PassiveLogCollector::ConnectJobTracker::ConnectJobTracker()
    : RequestTrackerBase(kMaxGraveyardSize) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::ConnectJobTracker::DoAddEntry(const Entry& entry,
                                                   RequestInfo* out_info) {
  AddEntryToRequestInfo(entry, is_unbounded(), out_info);

  if (entry.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID) {
    SetSubordinateSource(out_info, entry);
  }

  // If this is the end of the connect job, move the request to the graveyard.
  if (entry.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB &&
      entry.phase == net::NetLog::PHASE_END) {
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

void PassiveLogCollector::ConnectJobTracker::AppendLogEntries(
    RequestInfo* out_info, bool unbounded, uint32 connect_id) {
  RequestInfo* connect_info = GetRequestInfo(connect_id);
  if (!connect_info) {
    net::NetLogStringParameter* text = new net::NetLogStringParameter(
        "todo", StringPrintf("Used ConnectJob id=%u", connect_id));
    Entry new_entry(0, net::NetLog::TYPE_TODO_STRING, base::TimeTicks(),
                    net::NetLog::Source(net::NetLog::SOURCE_CONNECT_JOB,
                                        connect_id),
                    net::NetLog::PHASE_NONE, text);
    AddEntryToRequestInfo(new_entry, unbounded, out_info);
    return;
  }

  AppendToRequestInfo(*connect_info, unbounded, out_info);
  std::sort(out_info->entries.begin(), out_info->entries.end(),
            &SortByOrderComparator);
  out_info->num_entries_truncated += connect_info->num_entries_truncated;

  if (connect_info->subordinate_source.is_valid())
    AppendLogEntries(out_info, unbounded, connect_info->subordinate_source.id);
}

//----------------------------------------------------------------------------
// SocketTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::SocketTracker::kMaxGraveyardSize = 15;

PassiveLogCollector::SocketTracker::SocketTracker()
    : RequestTrackerBase(kMaxGraveyardSize) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::SocketTracker::DoAddEntry(const Entry& entry,
                                               RequestInfo* out_info) {
  int int_arg;
  switch (entry.type) {
    case net::NetLog::TYPE_SOCKET_BYTES_SENT:
      int_arg = static_cast<net::NetLogIntegerParameter*>(
          entry.params.get())->value();
      out_info->total_bytes_transmitted += int_arg;
      out_info->bytes_transmitted += int_arg;
      out_info->last_tx_rx_time = entry.time;
      out_info->last_tx_rx_position = entry.order;
      break;
    case net::NetLog::TYPE_SOCKET_BYTES_RECEIVED:
      int_arg = static_cast<net::NetLogIntegerParameter*>(
          entry.params.get())->value();
      out_info->total_bytes_received += int_arg;
      out_info->bytes_received += int_arg;
      out_info->last_tx_rx_time = entry.time;
      out_info->last_tx_rx_position = entry.order;
      break;
    case net::NetLog::TYPE_TCP_SOCKET_DONE:
      return ACTION_MOVE_TO_GRAVEYARD;
    default:
      AddEntryToRequestInfo(entry, is_unbounded(), out_info);
      break;
  }
  return ACTION_NONE;
}

void PassiveLogCollector::SocketTracker::AppendLogEntries(
    RequestInfo* out_info, bool unbounded, uint32 socket_id, bool clear) {
  RequestInfo* socket_info = GetRequestInfo(socket_id);
  if (!socket_info) {
    net::NetLogStringParameter* text = new net::NetLogStringParameter(
        "todo", StringPrintf("Used Socket id=%u.", socket_id));
    Entry new_entry(0, net::NetLog::TYPE_TODO_STRING, base::TimeTicks(),
                    net::NetLog::Source(net::NetLog::SOURCE_SOCKET, socket_id),
                    net::NetLog::PHASE_NONE, text);
    AddEntryToRequestInfo(new_entry, unbounded, out_info);
    return;
  }

  AppendToRequestInfo(*socket_info, unbounded, out_info);
  out_info->num_entries_truncated += socket_info->num_entries_truncated;

  // Synthesize a log entry for bytes sent and received.
  if (socket_info->bytes_transmitted > 0 || socket_info->bytes_received > 0) {
    net::NetLogStringParameter* text = new net::NetLogStringParameter(
        "stats",
        StringPrintf("Tx/Rx: %"PRIu64"/%"PRIu64" [%"PRIu64"/%"PRIu64
                     " total on socket] (Bytes)",
                     socket_info->bytes_transmitted,
                     socket_info->bytes_received,
                     socket_info->total_bytes_transmitted,
                     socket_info->total_bytes_received));
    Entry new_entry(socket_info->last_tx_rx_position,
                    net::NetLog::TYPE_TODO_STRING,
                    socket_info->last_tx_rx_time,
                    net::NetLog::Source(net::NetLog::SOURCE_SOCKET, socket_id),
                    net::NetLog::PHASE_NONE,
                    text);
    AddEntryToRequestInfo(new_entry, unbounded, out_info);
  }
  std::sort(out_info->entries.begin(), out_info->entries.end(),
            &SortByOrderComparator);

  if (clear)
    ClearInfo(socket_info);
}

void PassiveLogCollector::SocketTracker::ClearInfo(RequestInfo* info) {
  info->entries.clear();
  info->num_entries_truncated = 0;
  info->bytes_transmitted = 0;
  info->bytes_received = 0;
  info->last_tx_rx_position = 0;
  info->last_tx_rx_time = base::TimeTicks();
}

//----------------------------------------------------------------------------
// RequestTracker
//----------------------------------------------------------------------------

const size_t PassiveLogCollector::RequestTracker::kMaxGraveyardSize = 25;
const size_t PassiveLogCollector::RequestTracker::kMaxGraveyardURLSize = 1000;

PassiveLogCollector::RequestTracker::RequestTracker(
    ConnectJobTracker* connect_job_tracker, SocketTracker* socket_tracker)
    : RequestTrackerBase(kMaxGraveyardSize),
      connect_job_tracker_(connect_job_tracker),
      socket_tracker_(socket_tracker) {
}

PassiveLogCollector::RequestTrackerBase::Action
PassiveLogCollector::RequestTracker::DoAddEntry(const Entry& entry,
                                                RequestInfo* out_info) {
  // We expect up to three events with IDs.
  //   - Begin SOCKET_POOL_CONNECT_JOB_ID: Means a ConnectJob was created for
  //     this request.  Including it for now, but the resulting socket may be
  //     used for a different request.
  //   - End SOCKET_POOL_CONNECT_JOB_ID: The named ConnectJob completed and
  //     this request will be getting the socket from that request.
  //   - SOCKET_POOL_SOCKET_ID: The given socket will be used for this request.
  //
  // The action to take when seeing these events depends on the current
  // content of the |subordinate_source| field:
  // |subordinate_source| is invalid (fresh state).
  //   - Begin SOCKET_POOL_CONNECT_JOB_ID: Set |subordinate_source|.
  //   - End SOCKET_POOL_CONNECT_JOB_ID: Integrate the named ConnectJob ID.
  //   - SOCKET_POOL_SOCKET_ID: Set |subordinate_source|.
  // |subordinate_source| is a ConnectJob:
  //   - Begin SOCKET_POOL_CONNECT_JOB_ID: Set |subordinate_source|.
  //   - End SOCKET_POOL_CONNECT_JOB_ID: Integrate the named ConnectJob ID and
  //     clear the |subordinate_source|.
  //   - SOCKET_POOL_SOCKET_ID: Set |subordinate_source|. (The request was
  //     assigned a new idle socket, after starting a ConnectJob.)
  // |subordinate_source| is a Socket:
  //   First, integrate the subordinate socket source, then:
  //   - Begin SOCKET_POOL_CONNECT_JOB_ID: Set |subordinate_source|.
  //     (Connection restarted with a new ConnectJob.)
  //   - End SOCKET_POOL_CONNECT_JOB_ID: Integrate the named ConnectJob ID and
  //     clear the |subordinate_source|. (Connection restarted with a late bound
  //     ConnectJob.)
  //   - SOCKET_POOL_SOCKET_ID: Set |subordinate_source|. (Connection
  //     restarted and got an idle socket.)
  if (entry.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID ||
      entry.type == net::NetLog::TYPE_SOCKET_POOL_SOCKET_ID) {

    if (out_info->subordinate_source.is_valid() &&
        out_info->subordinate_source.type == net::NetLog::SOURCE_SOCKET)
      IntegrateSubordinateSource(out_info, true);

    SetSubordinateSource(out_info, entry);

    if (entry.phase == net::NetLog::PHASE_END &&
        entry.type == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID) {
      IntegrateSubordinateSource(out_info, true);
      out_info->subordinate_source.id = net::NetLog::Source::kInvalidId;
    }
  }

  AddEntryToRequestInfo(entry, is_unbounded(), out_info);

  // If this was the start of a URLRequest/SocketStream, extract the URL.
  // Note: we look at the first *two* entries, since the outer REQUEST_ALIVE
  // doesn't actually contain any data.
  if (out_info->url.empty() && out_info->entries.size() <= 2 &&
      entry.phase == net::NetLog::PHASE_BEGIN && entry.params &&
      (entry.type == net::NetLog::TYPE_URL_REQUEST_START ||
       entry.type == net::NetLog::TYPE_SOCKET_STREAM_CONNECT)) {
    out_info->url = static_cast<net::NetLogStringParameter*>(
        entry.params.get())->value();
  }

  // If the request has ended, move it to the graveyard.
  if (entry.type == net::NetLog::TYPE_REQUEST_ALIVE &&
      entry.phase == net::NetLog::PHASE_END) {
    IntegrateSubordinateSource(out_info, true);
    if (StartsWithASCII(out_info->url, "chrome://", false)) {
      // Avoid sending "chrome://" requests to the graveyard, since it just
      // adds to clutter.
      return ACTION_DELETE;
    }
    return ACTION_MOVE_TO_GRAVEYARD;
  }

  return ACTION_NONE;
}

void PassiveLogCollector::RequestTracker::IntegrateSubordinateSource(
    RequestInfo* info, bool clear_entries) const {
  if (!info->subordinate_source.is_valid())
    return;

  uint32 subordinate_id = info->subordinate_source.id;
  switch (info->subordinate_source.type) {
    case net::NetLog::SOURCE_CONNECT_JOB:
      connect_job_tracker_->AppendLogEntries(
          info, connect_job_tracker_->is_unbounded(), subordinate_id);
      break;
    case net::NetLog::SOURCE_SOCKET:
      socket_tracker_->AppendLogEntries(info, socket_tracker_->is_unbounded(),
                                        subordinate_id, clear_entries);
      break;
    default:
      NOTREACHED();
      break;
  }
}

//----------------------------------------------------------------------------
// InitProxyResolverTracker
//----------------------------------------------------------------------------

PassiveLogCollector::InitProxyResolverTracker::InitProxyResolverTracker() {}

void PassiveLogCollector::InitProxyResolverTracker::OnAddEntry(
    const Entry& entry) {
  if (entry.type == net::NetLog::TYPE_INIT_PROXY_RESOLVER &&
      entry.phase == net::NetLog::PHASE_BEGIN) {
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

