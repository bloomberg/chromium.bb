// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_REQUEST_TRACKER_DATA_MEMOIZING_STORE_H_
#define IOS_WEB_NET_REQUEST_TRACKER_DATA_MEMOIZING_STORE_H_

#include <map>

#include "base/bind.h"
#include "base/synchronization/lock.h"
#include "ios/web/public/web_thread.h"

namespace web {

// RequestTrackerDataMemoizingStore is a thread-safe container that retains
// reference counted objects that are associated with one or more request
// trackers. Objects are identified by an int and only a single reference to a
// given object is retained.
template <typename T>
class RequestTrackerDataMemoizingStore {
 public:
  RequestTrackerDataMemoizingStore() : next_item_id_(1) {}

  ~RequestTrackerDataMemoizingStore() {
    DCHECK_EQ(0U, id_to_item_.size()) << "Failed to outlive request tracker";
  }

  // Store adds |item| to this collection, associates it with the given request
  // tracker id and returns an opaque identifier for it. If |item| is already
  // known, the same identifier will be returned.
  int Store(T* item, int request_tracker_id) {
    DCHECK(item);
    base::AutoLock auto_lock(lock_);

    int item_id = 0;

    // Is this item alread known?
    typename ReverseItemMap::iterator item_iter = item_to_id_.find(item);
    if (item_iter == item_to_id_.end()) {
      item_id = next_item_id_++;
      // Use 0 as an invalid item_id value.  In the unlikely event that
      // next_item_id_ wraps around, reset it to 1.
      if (next_item_id_ == 0)
        next_item_id_ = 1;
      id_to_item_[item_id] = item;
      item_to_id_[item] = item_id;
    } else {
      item_id = item_iter->second;
    }

    // Update the bidirection request_tracker_id and item_id mappings.
    UpdateMap(&request_tracker_id_to_item_id_, request_tracker_id, item_id);
    UpdateMap(&item_id_to_request_tracker_id_, item_id, request_tracker_id);

    DCHECK(item_id);
    return item_id;
  }

  // Retrieves a previously Stored() item, identified by |item_id|.
  // If |item_id| is recognized, |item| will be updated and Retrieve() will
  // return true.
  bool Retrieve(int item_id, scoped_refptr<T>* item) {
    base::AutoLock auto_lock(lock_);

    typename ItemMap::iterator iter = id_to_item_.find(item_id);
    if (iter == id_to_item_.end())
      return false;
    if (item)
      *item = iter->second;
    return true;
  }

  // Removes any items associtated only with |request_tracker_id|.
  void RemoveForRequestTracker(int request_tracker_id) {
    RemoveRequestTrackerItems(request_tracker_id);
  }

 private:
  typedef std::multimap<int, int> IDMap;
  typedef std::map<int, scoped_refptr<T>> ItemMap;
  typedef std::map<T*, int, typename T::LessThan> ReverseItemMap;

  template <typename M>
  struct MatchSecond {
    explicit MatchSecond(const M& t) : value(t) {}

    template <typename Pair>
    bool operator()(const Pair& p) const {
      return (value == p.second);
    }

    M value;
  };

  // Updates |map| to contain a key_id->value_id pair, ensuring that a duplicate
  // pair is not added if it was already present.
  static void UpdateMap(IDMap* map, int key_id, int value_id) {
    auto matching_key_range = map->equal_range(key_id);
    if (std::find_if(matching_key_range.first, matching_key_range.second,
                     MatchSecond<int>(value_id)) == matching_key_range.second) {
      map->insert(std::make_pair(key_id, value_id));
    }
  }

  // Remove the item specified by |item_id| from id_to_item_ and item_to_id_.
  // NOTE: the caller (RemoveRequestTrackerItems) must hold lock_.
  void RemoveInternal(int item_id) {
    typename ItemMap::iterator item_iter = id_to_item_.find(item_id);
    DCHECK(item_iter != id_to_item_.end());

    typename ReverseItemMap::iterator id_iter =
        item_to_id_.find(item_iter->second.get());
    DCHECK(id_iter != item_to_id_.end());
    item_to_id_.erase(id_iter);

    id_to_item_.erase(item_iter);
  }

  // Removes all the items associated with the specified request tracker from
  // the store.
  void RemoveRequestTrackerItems(int request_tracker_id) {
    base::AutoLock auto_lock(lock_);

    // Iterate through all the item ids for that request tracker.
    auto request_tracker_ids =
        request_tracker_id_to_item_id_.equal_range(request_tracker_id);
    for (IDMap::iterator ids_iter = request_tracker_ids.first;
         ids_iter != request_tracker_ids.second; ++ids_iter) {
      int item_id = ids_iter->second;
      // Find all the request trackers referring to this item id in
      // item_id_to_request_tracker_id_, then locate the request tracker being
      // removed within that range.
      auto item_ids = item_id_to_request_tracker_id_.equal_range(item_id);
      IDMap::iterator proc_iter =
          std::find_if(item_ids.first, item_ids.second,
                       MatchSecond<int>(request_tracker_id));
      DCHECK(proc_iter != item_ids.second);

      // Before removing, determine if no other request trackers refer to the
      // current item id. If |proc_iter| (the current request tracker) is the
      // lower bound of request trackers containing the current item id and if
      // |next_proc_iter| is the upper bound (the first request tracker that
      // does not), then only one request tracker, the one being removed, refers
      // to the item id.
      IDMap::iterator next_proc_iter = proc_iter;
      ++next_proc_iter;
      bool last_request_tracker_for_item_id =
          (proc_iter == item_ids.first && next_proc_iter == item_ids.second);
      item_id_to_request_tracker_id_.erase(proc_iter);

      if (last_request_tracker_for_item_id) {
        // The current item id is not referenced by any other request trackers,
        // so remove it from id_to_item_ and item_to_id_.
        RemoveInternal(item_id);
      }
    }
    if (request_tracker_ids.first != request_tracker_ids.second)
      request_tracker_id_to_item_id_.erase(request_tracker_ids.first,
                                           request_tracker_ids.second);
  }

  // Bidirectional mapping between items and the request trackers they are
  // associated with.
  IDMap request_tracker_id_to_item_id_;
  IDMap item_id_to_request_tracker_id_;
  // Bidirectional mappings between items and the item-level identifiers that
  // have been assigned to them.
  ItemMap id_to_item_;
  ReverseItemMap item_to_id_;

  // The ID to assign to the next item stored.
  int next_item_id_;

  // This lock protects: request_tracker_id_to_item_id_,
  // item_id_to_request_tracker_id_, id_to_item_, and item_to_id_.
  base::Lock lock_;
};

}  // namespace web

#endif  // IOS_WEB_NET_REQUEST_TRACKER_DATA_MEMOIZING_STORE_H_
