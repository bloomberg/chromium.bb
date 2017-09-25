// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SELECTION_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SELECTION_H_

namespace viz {

template <typename BoundType>
struct Selection {
  Selection() = default;
  ~Selection() = default;

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

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SELECTION_H_
