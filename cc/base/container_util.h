// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_CONTAINER_UTIL_H_
#define CC_BASE_CONTAINER_UTIL_H_

#include "base/memory/scoped_ptr.h"

namespace cc {

// Removes the front element from the container and returns it. Note that this
// currently only works with types that implement Pass().
// TODO(vmpstr): Use std::move instead of Pass when allowed.
template <typename Container>
typename Container::value_type PopFront(Container* container) {
  typename Container::value_type element = container->front().Pass();
  container->pop_front();
  return element;
}

// Removes the back element from the container and returns it. Note that this
// currently only works with types that implement Pass().
// TODO(vmpstr): Use std::move instead of Pass when allowed.
template <typename Container>
typename Container::value_type PopBack(Container* container) {
  typename Container::value_type element = container->back().Pass();
  container->pop_back();
  return element;
}

}  // namespace cc

#endif  // CC_BASE_CONTAINER_UTIL_H_
