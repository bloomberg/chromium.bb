// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_WRITE_BLOCKED_LIST_H_
#define NET_SPDY_WRITE_BLOCKED_LIST_H_

#include <algorithm>
#include <deque>

#include "base/logging.h"

namespace net {

const int kHighestPriority = 0;
const int kLowestPriority = 7;

template <typename IdType>
class WriteBlockedList {
 public:
  // 0(1) size lookup.  0(1) insert at front or back.
  typedef std::deque<IdType> BlockedList;
  typedef typename BlockedList::iterator iterator;

  // Returns the priority of the highest priority list with sessions on it, or
  // -1 if none of the lists have pending sessions.
  int GetHighestPriorityWriteBlockedList() const {
    for (int i = 0; i <= kLowestPriority; ++i) {
      if (write_blocked_lists_[i].size() > 0)
        return i;
    }
    return -1;
  }

  int PopFront(int priority) {
    DCHECK(!write_blocked_lists_[priority].empty());
    IdType stream_id = write_blocked_lists_[priority].front();
    write_blocked_lists_[priority].pop_front();
    return stream_id;
  }

  bool HasWriteBlockedStreamsGreaterThanPriority(int priority) const {
    for (int i = kHighestPriority; i < priority; ++i) {
      if (!write_blocked_lists_[i].empty()) {
        return true;
      }
    }
    return false;
  }

  bool HasWriteBlockedStreams() const {
    return HasWriteBlockedStreamsGreaterThanPriority(kLowestPriority + 1);
  }

  void PushBack(IdType stream_id, int priority) {
    write_blocked_lists_[priority].push_back(stream_id);
  }

  void RemoveStreamFromWriteBlockedList(IdType stream_id, int priority) {
    iterator it = std::find(write_blocked_lists_[priority].begin(),
                            write_blocked_lists_[priority].end(),
                            stream_id);
    while (it != write_blocked_lists_[priority].end()) {
      write_blocked_lists_[priority].erase(it);
      it = std::find(write_blocked_lists_[priority].begin(),
                     write_blocked_lists_[priority].end(),
                     stream_id);
    }
  }

  int NumBlockedStreams() {
    int num_blocked_streams = 0;
    for (int i = kHighestPriority; i <= kLowestPriority; ++i) {
      num_blocked_streams += write_blocked_lists_[i].size();
    }
    return num_blocked_streams;
  }

 private:
  // Priority ranges from 0 to 7
  BlockedList write_blocked_lists_[8];
};

}  // namespace net

#endif  // NET_SPDY_WRITE_BLOCKED_LIST_H_
