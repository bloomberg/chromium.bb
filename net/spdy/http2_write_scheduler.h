// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HTTP2_WRITE_SCHEDULER_H_
#define NET_SPDY_HTTP2_WRITE_SCHEDULER_H_

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/linked_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "net/spdy/spdy_bug_tracker.h"

namespace net {

// This data structure implements the HTTP/2 stream priority tree defined in
// section 5.3 of RFC 7540:
// http://tools.ietf.org/html/rfc7540#section-5.3
//
// Streams can be added and removed, and dependencies between them defined.
// Streams constitute a tree rooted at stream ID 0: each stream has a single
// parent stream, and 0 or more child streams.  Individual streams can be
// marked as ready to read/write, and then the whole structure can be queried
// to pick the next stream to read/write out of those that are ready.
//
// The StreamIdType type must be a POD that supports comparison (most
// likely, it will be a number).

namespace test {
template <typename StreamIdType>
class Http2PriorityWriteSchedulerPeer;
}

const unsigned int kHttp2RootStreamId = 0;
const int kHttp2DefaultStreamWeight = 16;
const int kHttp2MinStreamWeight = 1;
const int kHttp2MaxStreamWeight = 256;

template <typename StreamIdType>
class Http2PriorityWriteScheduler {
 public:
  Http2PriorityWriteScheduler();

  friend class test::Http2PriorityWriteSchedulerPeer<StreamIdType>;

  // Return the number of streams currently in the tree.
  int num_streams() const;

  // Return true if the tree contains a stream with the given ID.
  bool StreamRegistered(StreamIdType stream_id) const;

  // Registers a new stream with the given weight and parent, adding it to the
  // dependency tree. Non-exclusive streams simply get added below the parent
  // stream. If exclusive = true, the stream becomes the parent's sole child
  // and the parent's previous children become the children of the new stream.
  // If the stream was already registered, logs SPDY_BUG and does nothing. If
  // the parent stream is not registered, logs SPDY_BUG and uses the root stream
  // as the parent.
  void RegisterStream(StreamIdType stream_id,
                      StreamIdType parent_id,
                      int weight,
                      bool exclusive);

  // Unregisters the given stream from the scheduler, removing it from the
  // dependency tree. If the stream was not previously registered, logs
  // SPDY_BUG and does nothing.
  void UnregisterStream(StreamIdType stream_id);

  // Returns the weight value for the specified stream. If the stream is not
  // registered, logs SPDY_BUG and returns the lowest weight.
  int GetStreamWeight(StreamIdType stream_id) const;

  // Returns the stream ID for the parent of the given stream. If the stream
  // isn't registered, logs SPDY_BUG and returns the root stream ID (0).
  StreamIdType GetStreamParent(StreamIdType stream_id) const;

  // Returns stream IDs of the children of the given stream, if any. If the
  // stream isn't registered, logs SPDY_BUG and returns an empty vector.
  std::vector<StreamIdType> GetStreamChildren(StreamIdType stream_id) const;

  // Sets the weight of the given stream. If the stream isn't registered or is
  // the root stream, logs SPDY_BUG and does nothing.
  void SetStreamWeight(StreamIdType stream_id, int weight);

  // Sets the parent of the given stream. If the stream and/or parent aren't
  // registered, logs SPDY_BUG and does nothing. If the new parent is a
  // descendant of the stream (i.e. this would have created a cycle) then the
  // topology of the tree is rearranged as described in section 5.3.3 of RFC
  // 7540: https://tools.ietf.org/html/rfc7540#section-5.3.3
  void SetStreamParent(StreamIdType stream_id,
                       StreamIdType parent_id,
                       bool exclusive);

  // Returns true if the stream parent_id has child_id in its children. If
  // either parent or child stream aren't registered, logs SPDY_BUG and returns
  // false.
  bool StreamHasChild(StreamIdType parent_id, StreamIdType child_id) const;

  // Marks the stream as blocked or unblocked. If the stream is not registered,
  // logs SPDY_BUG and does nothing.
  void MarkStreamBlocked(StreamIdType stream_id, bool blocked);

  // Marks the stream as ready or not ready to write; i.e. whether there is
  // buffered data for the associated stream. If the stream is not registered,
  // logs SPDY_BUG and does nothing.
  void MarkStreamReady(StreamIdType stream_id, bool ready);

  // Returns true iff the scheduler has one or more usable streams. A stream is
  // usable if it has ready == true and blocked == false, and is not the direct
  // or indirect child of another stream that itself has ready == true and
  // blocked == false. (Note that the root stream always has ready == false.)
  bool HasUsableStreams() const;

  // If the scheduler has any usable streams, returns the ID of the next usable
  // stream, in the process changing its ready state to false. If the scheduler
  // does not have any usable streams, logs SPDY_BUG and returns the root stream
  // ID (0). If there are multiple usable streams, precedence is given to the
  // one with the highest priority (thus preserving SPDY priority semantics),
  // or, if there are multiple with the highest priority, the one with the
  // lowest ordinal (ensuring round-robin ordering).
  StreamIdType PopNextUsableStream();

 private:
  struct StreamInfo;
  using StreamInfoVector = std::vector<StreamInfo*>;
  using StreamInfoMap = std::unordered_map<StreamIdType, StreamInfo*>;

  struct StreamInfo : public base::LinkNode<StreamInfo> {
    // ID for this stream.
    StreamIdType id;
    // ID of parent stream.
    StreamInfo* parent = nullptr;
    // Weights can range between 1 and 256 (inclusive).
    int weight = kHttp2DefaultStreamWeight;
    // The total weight of this stream's direct descendants.
    int total_child_weights = 0;
    // Pointers to StreamInfos for children, if any.
    StreamInfoVector children;
    // Is the associated stream write-blocked?
    bool blocked = false;
    // Does the stream have data ready for writing?
    bool ready = false;
    // Whether the stream is currently present in scheduling_queue_.
    bool scheduled = false;
    // The scheduling priority of this stream. Streams with higher priority
    // values are scheduled first.
    float priority = 0;
    // Ordinal value for this stream, used to ensure round-robin scheduling:
    // among streams with the same scheduling priority, streams with lower
    // ordinal are scheduled first. The ordinal is reset to a new, greater
    // value when the stream is next inserted into scheduling_queue_.
    int64_t ordinal = 0;

    // Whether the stream ought to be in scheduling_queue_.
    bool IsSchedulable() const { return ready && !blocked; }

    // Whether this stream should be scheduled ahead of another stream.
    bool SchedulesBefore(const StreamInfo& other) const {
      return (priority != other.priority) ? priority > other.priority
                                          : ordinal < other.ordinal;
    }
  };

  static bool Remove(StreamInfoVector* stream_infos,
                     const StreamInfo* stream_info);

  // Clamps weight to a value in [kHttp2MinStreamWeight,
  // kHttp2MaxStreamWeight].
  static int ClampWeight(int weight);

  // Returns true iff any direct or transitive parent of the given stream is
  // currently scheduled.
  static bool HasScheduledAncestor(const StreamInfo& stream_info);

  // Returns StreamInfo for the given stream, or nullptr if it isn't
  // registered.
  const StreamInfo* FindStream(StreamIdType stream_id) const;
  StreamInfo* FindStream(StreamIdType stream_id);

  // Update all priority values in the subtree rooted at the given stream, not
  // including the stream itself. If this results in priority value changes for
  // scheduled streams, those streams are rescheduled to ensure proper ordering
  // of scheduling_queue_.
  void UpdatePrioritiesUnder(StreamInfo* stream_info);

  // Adds or removes stream from scheduling_queue_ according to whether it is
  // schedulable. If stream is newly schedulable, assigns it the next
  // (increasing) ordinal value.
  void UpdateScheduling(StreamInfo* stream_info);

  // Inserts stream into scheduling_queue_ at the appropriate location given
  // its priority and ordinal. Time complexity is O(scheduling_queue.size()).
  void Schedule(StreamInfo* stream_info);

  // Removes stream from scheduling_queue_.
  void Unschedule(StreamInfo* stream_info);

  // Return true if all internal invariants hold (useful for unit tests).
  // Unless there are bugs, this should always return true.
  bool ValidateInvariantsForTests() const;

  // Pointee owned by all_stream_infos_.
  StreamInfo* root_stream_info_;
  // Maps from stream IDs to StreamInfo objects.
  StreamInfoMap all_stream_infos_;
  STLValueDeleter<StreamInfoMap> all_stream_infos_deleter_;
  // Queue containing all streams that are ready and unblocked, ordered with
  // streams of higher priority before streams of lower priority, and, among
  // streams of equal priority, streams with lower ordinal before those with
  // higher ordinal. Note that not all streams in scheduling_queue_ are
  // necessarily usable: some may have ancestor stream(s) that are ready and
  // unblocked. In these situations the occluded child streams are left in the
  // queue, to reduce churn.
  base::LinkedList<StreamInfo> scheduling_queue_;
  // Ordinal value to assign to next node inserted into
  // scheduling_queue_. Incremented after each insertion.
  int64_t next_ordinal_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Http2PriorityWriteScheduler);
};

template <typename StreamIdType>
Http2PriorityWriteScheduler<StreamIdType>::Http2PriorityWriteScheduler()
    : all_stream_infos_deleter_(&all_stream_infos_) {
  root_stream_info_ = new StreamInfo();
  root_stream_info_->id = kHttp2RootStreamId;
  root_stream_info_->weight = kHttp2DefaultStreamWeight;
  root_stream_info_->parent = nullptr;
  root_stream_info_->priority = 1.0;
  root_stream_info_->ready = true;
  all_stream_infos_[kHttp2RootStreamId] = root_stream_info_;
}

template <typename StreamIdType>
int Http2PriorityWriteScheduler<StreamIdType>::num_streams() const {
  return all_stream_infos_.size();
}

template <typename StreamIdType>
bool Http2PriorityWriteScheduler<StreamIdType>::StreamRegistered(
    StreamIdType stream_id) const {
  return ContainsKey(all_stream_infos_, stream_id);
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::RegisterStream(
    StreamIdType stream_id,
    StreamIdType parent_id,
    int weight,
    bool exclusive) {
  if (StreamRegistered(stream_id)) {
    SPDY_BUG << "Stream " << stream_id << " already registered";
    return;
  }
  weight = ClampWeight(weight);

  StreamInfo* parent = FindStream(parent_id);
  if (parent == nullptr) {
    SPDY_BUG << "Parent stream " << parent_id << " not registered";
    parent = root_stream_info_;
  }

  StreamInfo* new_stream_info = new StreamInfo;
  new_stream_info->id = stream_id;
  new_stream_info->weight = weight;
  new_stream_info->parent = parent;
  all_stream_infos_[stream_id] = new_stream_info;
  if (exclusive) {
    // Move the parent's current children below the new stream.
    using std::swap;
    swap(new_stream_info->children, parent->children);
    new_stream_info->total_child_weights = parent->total_child_weights;
    // Update each child's parent.
    for (StreamInfo* child : new_stream_info->children) {
      child->parent = new_stream_info;
    }
    // Clear parent's old child data.
    DCHECK(parent->children.empty());
    parent->total_child_weights = 0;
  }
  // Add new stream to parent.
  parent->children.push_back(new_stream_info);
  parent->total_child_weights += weight;

  // Update all priorities under parent, since addition of a stream affects
  // sibling priorities as well.
  UpdatePrioritiesUnder(parent);

  // Stream starts with ready == false, so no need to schedule it yet.
  DCHECK(!new_stream_info->IsSchedulable());
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::UnregisterStream(
    StreamIdType stream_id) {
  if (stream_id == kHttp2RootStreamId) {
    SPDY_BUG << "Cannot unregister root stream";
    return;
  }
  // Remove the stream from table.
  typename StreamInfoMap::iterator it = all_stream_infos_.find(stream_id);
  if (it == all_stream_infos_.end()) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
    return;
  }
  std::unique_ptr<StreamInfo> stream_info(std::move(it->second));
  all_stream_infos_.erase(it);
  // If scheduled, unschedule.
  if (stream_info->scheduled) {
    Unschedule(stream_info.get());
  }

  StreamInfo* parent = stream_info->parent;
  // Remove the stream from parent's child list.
  Remove(&parent->children, stream_info.get());
  parent->total_child_weights -= stream_info->weight;

  // Move the stream's children to the parent's child list.
  // Update each child's parent and weight.
  for (StreamInfo* child : stream_info->children) {
    child->parent = parent;
    parent->children.push_back(child);
    // Divide the removed stream's weight among its children, rounding to the
    // nearest valid weight.
    float float_weight = stream_info->weight *
                         static_cast<float>(child->weight) /
                         static_cast<float>(stream_info->total_child_weights);
    int new_weight = floor(float_weight + 0.5);
    if (new_weight == 0) {
      new_weight = 1;
    }
    child->weight = new_weight;
    parent->total_child_weights += child->weight;
  }
  UpdatePrioritiesUnder(parent);
}

template <typename StreamIdType>
int Http2PriorityWriteScheduler<StreamIdType>::GetStreamWeight(
    StreamIdType stream_id) const {
  const StreamInfo* stream_info = FindStream(stream_id);
  if (stream_info == nullptr) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
    return kHttp2MinStreamWeight;
  }
  return stream_info->weight;
}

template <typename StreamIdType>
StreamIdType Http2PriorityWriteScheduler<StreamIdType>::GetStreamParent(
    StreamIdType stream_id) const {
  const StreamInfo* stream_info = FindStream(stream_id);
  if (stream_info == nullptr) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
    return kHttp2RootStreamId;
  }
  if (stream_info->parent == nullptr) {  // root stream
    return kHttp2RootStreamId;
  }
  return stream_info->parent->id;
}

template <typename StreamIdType>
std::vector<StreamIdType> Http2PriorityWriteScheduler<
    StreamIdType>::GetStreamChildren(StreamIdType stream_id) const {
  std::vector<StreamIdType> child_vec;
  const StreamInfo* stream_info = FindStream(stream_id);
  if (stream_info == nullptr) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
  } else {
    child_vec.reserve(stream_info->children.size());
    for (StreamInfo* child : stream_info->children) {
      child_vec.push_back(child->id);
    }
  }
  return child_vec;
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::SetStreamWeight(
    StreamIdType stream_id,
    int weight) {
  if (stream_id == kHttp2RootStreamId) {
    SPDY_BUG << "Cannot set weight of root stream";
    return;
  }
  StreamInfo* stream_info = FindStream(stream_id);
  if (stream_info == nullptr) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
    return;
  }
  weight = ClampWeight(weight);
  if (weight == stream_info->weight) {
    return;
  }
  if (stream_info->parent != nullptr) {
    stream_info->parent->total_child_weights += (weight - stream_info->weight);
  }
  stream_info->weight = weight;

  // Change in weight also affects sibling priorities.
  UpdatePrioritiesUnder(stream_info->parent);
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::SetStreamParent(
    StreamIdType stream_id,
    StreamIdType parent_id,
    bool exclusive) {
  if (stream_id == kHttp2RootStreamId) {
    SPDY_BUG << "Cannot set parent of root stream";
    return;
  }
  if (stream_id == parent_id) {
    SPDY_BUG << "Cannot set stream to be its own parent";
    return;
  }
  StreamInfo* stream_info = FindStream(stream_id);
  if (stream_info == nullptr) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
    return;
  }
  StreamInfo* new_parent = FindStream(parent_id);
  if (new_parent == nullptr) {
    SPDY_BUG << "Parent stream " << parent_id << " not registered";
    return;
  }

  // If the new parent is already the stream's parent, we're done.
  if (stream_info->parent == new_parent) {
    return;
  }

  // Next, check to see if the new parent is currently a descendant
  // of the stream.
  StreamInfo* last = new_parent->parent;
  bool cycle_exists = false;
  while (last != nullptr) {
    if (last == stream_info) {
      cycle_exists = true;
      break;
    }
    last = last->parent;
  }

  if (cycle_exists) {
    // The new parent moves to the level of the current stream.
    SetStreamParent(parent_id, stream_info->parent->id, false);
  }

  // Remove stream from old parent's child list.
  StreamInfo* old_parent = stream_info->parent;
  Remove(&old_parent->children, stream_info);
  old_parent->total_child_weights -= stream_info->weight;
  UpdatePrioritiesUnder(old_parent);

  if (exclusive) {
    // Move the new parent's current children below the current stream.
    for (StreamInfo* child : new_parent->children) {
      child->parent = stream_info;
      stream_info->children.push_back(child);
    }
    stream_info->total_child_weights += new_parent->total_child_weights;
    // Clear new parent's old child data.
    new_parent->children.clear();
    new_parent->total_child_weights = 0;
  }

  // Make the change.
  stream_info->parent = new_parent;
  new_parent->children.push_back(stream_info);
  new_parent->total_child_weights += stream_info->weight;
  UpdatePrioritiesUnder(new_parent);
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::MarkStreamBlocked(
    StreamIdType stream_id,
    bool blocked) {
  if (stream_id == kHttp2RootStreamId) {
    SPDY_BUG << "Cannot mark root stream blocked or unblocked";
    return;
  }
  StreamInfo* stream_info = FindStream(stream_id);
  if (stream_info == nullptr) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
    return;
  }
  stream_info->blocked = blocked;
  UpdateScheduling(stream_info);
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::MarkStreamReady(
    StreamIdType stream_id,
    bool ready) {
  if (stream_id == kHttp2RootStreamId) {
    SPDY_BUG << "Cannot mark root stream ready or unready";
    return;
  }
  StreamInfo* stream_info = FindStream(stream_id);
  if (stream_info == nullptr) {
    SPDY_BUG << "Stream " << stream_id << " not registered";
    return;
  }
  stream_info->ready = ready;
  UpdateScheduling(stream_info);
}

template <typename StreamIdType>
bool Http2PriorityWriteScheduler<StreamIdType>::Remove(
    StreamInfoVector* stream_infos,
    const StreamInfo* stream_info) {
  for (typename StreamInfoVector::iterator it = stream_infos->begin();
       it != stream_infos->end(); ++it) {
    if (*it == stream_info) {
      stream_infos->erase(it);
      return true;
    }
  }
  return false;
}

template <typename StreamIdType>
int Http2PriorityWriteScheduler<StreamIdType>::ClampWeight(int weight) {
  if (weight < kHttp2MinStreamWeight) {
    SPDY_BUG << "Invalid weight: " << weight;
    return kHttp2MinStreamWeight;
  }
  if (weight > kHttp2MaxStreamWeight) {
    SPDY_BUG << "Invalid weight: " << weight;
    return kHttp2MaxStreamWeight;
  }
  return weight;
}

template <typename StreamIdType>
bool Http2PriorityWriteScheduler<StreamIdType>::HasScheduledAncestor(
    const StreamInfo& stream_info) {
  for (const StreamInfo* parent = stream_info.parent; parent != nullptr;
       parent = parent->parent) {
    if (parent->scheduled) {
      return true;
    }
  }
  return false;
}

template <typename StreamIdType>
const typename Http2PriorityWriteScheduler<StreamIdType>::StreamInfo*
Http2PriorityWriteScheduler<StreamIdType>::FindStream(
    StreamIdType stream_id) const {
  typename StreamInfoMap::const_iterator it = all_stream_infos_.find(stream_id);
  return it == all_stream_infos_.end() ? nullptr : it->second;
}

template <typename StreamIdType>
typename Http2PriorityWriteScheduler<StreamIdType>::StreamInfo*
Http2PriorityWriteScheduler<StreamIdType>::FindStream(StreamIdType stream_id) {
  typename StreamInfoMap::iterator it = all_stream_infos_.find(stream_id);
  return it == all_stream_infos_.end() ? nullptr : it->second;
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::UpdatePrioritiesUnder(
    StreamInfo* stream_info) {
  for (StreamInfo* child : stream_info->children) {
    child->priority = stream_info->priority *
                      (static_cast<float>(child->weight) /
                       static_cast<float>(stream_info->total_child_weights));
    if (child->scheduled) {
      // Reposition in scheduling_queue_. Use post-order for scheduling, to
      // benefit from the fact that children have priority <= parent priority.
      Unschedule(child);
      UpdatePrioritiesUnder(child);
      Schedule(child);
    } else {
      UpdatePrioritiesUnder(child);
    }
  }
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::UpdateScheduling(
    StreamInfo* stream_info) {
  if (stream_info->IsSchedulable() != stream_info->scheduled) {
    if (stream_info->scheduled) {
      Unschedule(stream_info);
    } else {
      stream_info->ordinal = next_ordinal_++;
      Schedule(stream_info);
    }
  }
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::Schedule(
    StreamInfo* stream_info) {
  DCHECK(!stream_info->scheduled);
  for (base::LinkNode<StreamInfo>* s = scheduling_queue_.head();
       s != scheduling_queue_.end(); s = s->next()) {
    if (stream_info->SchedulesBefore(*s->value())) {
      stream_info->InsertBefore(s);
      stream_info->scheduled = true;
      break;
    }
  }
  if (!stream_info->scheduled) {
    stream_info->InsertAfter(scheduling_queue_.tail());
    stream_info->scheduled = true;
  }
}

template <typename StreamIdType>
void Http2PriorityWriteScheduler<StreamIdType>::Unschedule(
    StreamInfo* stream_info) {
  DCHECK(stream_info->scheduled);
  stream_info->RemoveFromList();
  stream_info->scheduled = false;
}

template <typename StreamIdType>
bool Http2PriorityWriteScheduler<StreamIdType>::StreamHasChild(
    StreamIdType parent_id,
    StreamIdType child_id) const {
  const StreamInfo* parent = FindStream(parent_id);
  if (parent == nullptr) {
    SPDY_BUG << "Parent stream " << parent_id << " not registered";
    return false;
  }
  if (!StreamRegistered(child_id)) {
    SPDY_BUG << "Child stream " << child_id << " not registered";
    return false;
  }
  auto found = std::find_if(parent->children.begin(), parent->children.end(),
                            [child_id](StreamInfo* stream_info) {
                              return stream_info->id == child_id;
                            });
  return found != parent->children.end();
}

template <typename StreamIdType>
bool Http2PriorityWriteScheduler<StreamIdType>::HasUsableStreams() const {
  // Even though not every stream in scheduling queue is guaranteed to be
  // usable (since children are occluded by parents), the presence of any
  // streams guarantees at least one is usable.
  return !scheduling_queue_.empty();
}

template <typename StreamIdType>
StreamIdType Http2PriorityWriteScheduler<StreamIdType>::PopNextUsableStream() {
  for (base::LinkNode<StreamInfo>* s = scheduling_queue_.head();
       s != scheduling_queue_.end(); s = s->next()) {
    StreamInfo* stream_info = s->value();
    if (!HasScheduledAncestor(*stream_info)) {
      stream_info->ready = false;
      Unschedule(stream_info);
      return stream_info->id;
    }
  }
  SPDY_BUG << "No usable streams";
  return kHttp2RootStreamId;
}

template <typename StreamIdType>
bool Http2PriorityWriteScheduler<StreamIdType>::ValidateInvariantsForTests()
    const {
  int total_streams = 0;
  int streams_visited = 0;
  // Iterate through all streams in the map.
  for (const auto& kv : all_stream_infos_) {
    ++total_streams;
    ++streams_visited;
    const StreamInfo& stream_info = *kv.second;
    // All streams except the root should have a parent, and should appear in
    // the children of that parent.
    if (stream_info.id != kHttp2RootStreamId &&
        !StreamHasChild(stream_info.parent->id, stream_info.id)) {
      DLOG(INFO) << "Parent stream " << stream_info.parent->id
                 << " is not registered, or does not list stream "
                 << stream_info.id << " as its child.";
      return false;
    }

    if (!stream_info.children.empty()) {
      int total_child_weights = 0;
      // Iterate through the stream's children.
      for (StreamInfo* child : stream_info.children) {
        ++streams_visited;
        // Each stream in the list should exist and should have this stream
        // set as its parent.
        if (!StreamRegistered(child->id) ||
            stream_info.id != GetStreamParent(child->id)) {
          DLOG(INFO) << "Child stream " << child->id << " is not registered, "
                     << "or does not list " << stream_info.id
                     << " as its parent.";
          return false;
        }
        total_child_weights += child->weight;
      }
      // Verify that total_child_weights is correct.
      if (total_child_weights != stream_info.total_child_weights) {
        DLOG(INFO) << "Child weight totals do not agree. For stream "
                   << stream_info.id << " total_child_weights has value "
                   << stream_info.total_child_weights << ", expected "
                   << total_child_weights;
        return false;
      }
    }
  }

  // Make sure num_streams reflects the total number of streams the map
  // contains.
  if (total_streams != num_streams()) {
    DLOG(INFO) << "Map contains incorrect number of streams.";
    return false;
  }
  // Validate the validation function; we should have visited each stream twice
  // (except for the root)
  DCHECK(streams_visited == 2 * num_streams() - 1);
  return true;
}

}  // namespace net

#endif  // NET_SPDY_HTTP2_WRITE_SCHEDULER_H_
