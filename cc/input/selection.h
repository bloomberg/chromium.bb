// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SELECTION_H_
#define CC_INPUT_SELECTION_H_

#include "cc/base/cc_export.h"

namespace cc {

template <typename BoundType>
struct CC_EXPORT Selection {
  Selection() {}
  ~Selection() {}

  BoundType start, end;
};

template <typename BoundType>
inline bool operator==(const Selection<BoundType>& lhs,
                       const Selection<BoundType>& rhs) {
  return lhs.start == rhs.start && lhs.end == rhs.end;
}

template <typename BoundType>
inline bool operator!=(const Selection<BoundType>& lhs,
                       const Selection<BoundType>& rhs) {
  return !(lhs == rhs);
}

}  // namespace cc

#endif  // CC_INPUT_SELECTION_H_
