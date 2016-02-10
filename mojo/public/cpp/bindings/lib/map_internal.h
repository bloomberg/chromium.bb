// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_INTERNAL_H_

#include <stddef.h>
#include <map>
#include <utility>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {
namespace internal {

template <typename Key, typename Value, bool kValueIsMoveOnlyType>
struct MapTraits {};

// Defines traits of a map for which Value is not a move-only type.
template <typename Key, typename Value>
struct MapTraits<Key, Value, false> {
  static inline void Clone(const std::map<Key, Value>& src,
                           std::map<Key, Value>* dst) {
    dst->clear();
    for (auto it = src.begin(); it != src.end(); ++it)
      dst->insert(*it);
  }
};

// Defines traits of a map for which Value is a move-only type.
template <typename Key, typename Value>
struct MapTraits<Key, Value, true> {
  static inline void Clone(const std::map<Key, Value>& src,
                           std::map<Key, Value>* dst) {
    dst->clear();
    for (auto it = src.begin(); it != src.end(); ++it)
      dst->insert(std::make_pair(it->first, it->second.Clone()));
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_INTERNAL_H_
