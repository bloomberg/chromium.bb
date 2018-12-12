// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/scope_lock_range.h"

#include <ostream>

namespace content {

ScopeLockRange::ScopeLockRange(std::string begin, std::string end)
    : begin(std::move(begin)), end(std::move(end)) {}

std::ostream& operator<<(std::ostream& out, const ScopeLockRange& range) {
  return out << "<ScopeLockRange>{begin: " << range.begin
             << ", end: " << range.end << "}";
}

bool operator<(const ScopeLockRange& x, const ScopeLockRange& y) {
  if (x.begin != y.begin)
    return x.begin < y.begin;
  return x.end < y.end;
}

bool operator==(const ScopeLockRange& x, const ScopeLockRange& y) {
  return x.begin == y.begin && x.end == y.end;
}
bool operator!=(const ScopeLockRange& x, const ScopeLockRange& y) {
  return !(x == y);
}

}  // namespace content
