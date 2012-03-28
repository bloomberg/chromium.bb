// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_HASH_TABLES_H_
#define GPU_COMMAND_BUFFER_CLIENT_HASH_TABLES_H_

#if defined(__native_client__)
#include <tr1/unordered_map>
namespace gpu {
template <typename key, typename value>
struct hash_map : public std::tr1::unordered_map<key, value> {
};
}
#else
#include "base/hash_tables.h"
namespace gpu {
template <typename key, typename value>
struct hash_map : public base::hash_map<key, value> {
};
}
#endif

#endif  // GPU_COMMAND_BUFFER_CLIENT_HASH_TABLES_H_
