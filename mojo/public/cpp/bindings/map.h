// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_H_

#include <map>
#include <unordered_map>
#include <utility>

namespace mojo {

// TODO(yzshen): These conversion functions should be removed and callsites
// should be revisited and changed to use the same map type.
template <typename Key, typename Value>
std::unordered_map<Key, Value> MapToUnorderedMap(
    const std::map<Key, Value>& input) {
  return std::unordered_map<Key, Value>(input.begin(), input.end());
}

template <typename Key, typename Value>
std::unordered_map<Key, Value> MapToUnorderedMap(std::map<Key, Value>&& input) {
  return std::unordered_map<Key, Value>(std::make_move_iterator(input.begin()),
                                        std::make_move_iterator(input.end()));
}

template <typename Key, typename Value>
std::map<Key, Value> UnorderedMapToMap(
    const std::unordered_map<Key, Value>& input) {
  return std::map<Key, Value>(input.begin(), input.end());
}

template <typename Key, typename Value>
std::map<Key, Value> UnorderedMapToMap(std::unordered_map<Key, Value>&& input) {
  return std::map<Key, Value>(std::make_move_iterator(input.begin()),
                              std::make_move_iterator(input.end()));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_H_
