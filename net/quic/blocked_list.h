// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A combined list/hash set for read or write-blocked entities.

#ifndef NET_QUIC_BLOCKED_LIST_H_
#define NET_QUIC_BLOCKED_LIST_H_

#include <list>

#include "base/hash_tables.h"
#include "base/logging.h"

namespace net {

template <typename Object>
class BlockedList {
 public:
  // Called to add an object to the blocked list. This indicates
  // the object should be notified when it can use the socket again.
  //
  // If this object is already on the list, it will not be added again.
  void AddBlockedObject(Object object) {
    // Only add the object to the list if we successfully add it to the set.
    if (object_set_.insert(object).second) {
      object_list_.push_back(object);
    }
  }

  // Called to remove an object from a blocked list.  This should be
  // called in the event the object is being deleted before the list is.
  void RemoveBlockedObject(Object object) {
    // Remove the object from the set.  We'll check the set before calling
    // OnCanWrite on a object from the list.
    //
    // There is potentially ordering unfairness should a session be removed and
    // then readded (as it keeps its position in the list) but it's not worth
    // the overhead to walk the list and remove it.
    object_set_.erase(object);
  }

  // Called when the socket is usable and some objects can access it.  Returns
  // the first object and removes it from the list.
  Object GetNextBlockedObject() {
    DCHECK(!IsEmpty());

    // Walk the list to find the first object which was not removed from the
    // set.
    while (!object_list_.empty()) {
      Object object = *object_list_.begin();
      object_list_.pop_front();
      int removed = object_set_.erase(object);
      if (removed > 0) {
        return object;
      }
    }

    // This is a bit of a hack: It's illegal to call GetNextBlockedObject() if
    // the list is empty (see DCHECK above) but we must return something.  This
    // compiles for ints (returns 0) and pointers in the case that someone has a
    // bug in their call site.
    return 0;
  };

  // Returns the number of objects in the blocked list.
  int NumObjects() {
    return object_set_.size();
  };

  // Returns true if there are no objects in the list, false otherwise.
  bool IsEmpty() {
    return object_set_.empty();
  };

 private:
  // A set tracking the objects. This is the authoritative container for
  // determining if an object is blocked.   Objects in the list will always
  // be in the set.
  base::hash_set<Object> object_set_;
  // A list tracking the order in which objects were added to the list.
  // Objects are added to the back and pulled off the front, but only get
  // resumption calls if they're still in the set.
  // It's possible to be in the list twice, but only the first entry will get an
  // OnCanWrite call.
  std::list<Object> object_list_;
};

}  // namespace net

#endif  // NET_QUIC_BLOCKED_LIST_H_
