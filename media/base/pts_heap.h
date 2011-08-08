// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PTS_HEAP_H_
#define MEDIA_BASE_PTS_HEAP_H_

// The compressed frame are often in decode timestamp (dts) order, which
// may not always be in presentation timestamp (pts) order. However, the
// decoded frames will always be returned in pts order. Ideally, the pts could
// be attached as metadata to each comprsesed frame, and the decoder would
// pass it along in the uncompressed frame so that it can be used in render.
// Some decoders, like FFmpeg, do not have this facility, so we use PtsHeap to
// pass along this information and simulate the reodering.
//
// Here is an  illustration of why the reordering might be necessary.
//
//  Decode       PTS of         PTS of
//  Call #      Buffer In     Buffer Out
//    1             1             1
//    2             3            --- <--- frame 3 buffered by Decoder
//    3             2             2
//    4             4             3  <--- copying timestamp 4 and 6 would be
//    5             6             4  <-'  incorrect, which is why we sort and
//    6             5             5       queue incoming timestamps
//
// The PtsHeap expects that for every Pop(), there was a corresponding Push().
// It will CHECK fail otherwise.

#include <queue>
#include <vector>

#include "base/time.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT PtsHeap {
 public:
  PtsHeap();
  ~PtsHeap();

  void Push(const base::TimeDelta& pts);
  void Pop();

  const base::TimeDelta& Top() const { return queue_.top(); }
  bool IsEmpty() const { return queue_.empty(); }

 private:
  struct PtsHeapOrdering {
    bool operator()(const base::TimeDelta& lhs,
                    const base::TimeDelta& rhs) const {
      // std::priority_queue is a max-heap. We want lower timestamps to show up
      // first so reverse the natural less-than comparison.
      return rhs < lhs;
    }
  };
  typedef std::priority_queue<base::TimeDelta,
                              std::vector<base::TimeDelta>,
                              PtsHeapOrdering> TimeQueue;

  TimeQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(PtsHeap);
};

}  // namespace media

#endif  // MEDIA_BASE_PTS_HEAP_H_
