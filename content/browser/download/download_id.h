// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ID_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ID_H_
#pragma once

#include <iosfwd>

#include "base/hash_tables.h"
#include "content/common/content_export.h"

class DownloadManager;

// DownloadId combines per-profile Download ids with an indication of which
// profile in order to be globally unique. DownloadIds are not persistent across
// sessions, but their local() field is.
class DownloadId {
 public:
  static DownloadId Invalid() { return DownloadId(NULL, -1); }

  DownloadId(const DownloadManager* manager, int32 local_id)
    : manager_(manager),
      local_id_(local_id) {
  }

  // Return the per-profile and persistent part of this DownloadId.
  int32 local() const { return local_id_; }

  // Returns true if this DownloadId has been allocated and could possibly refer
  // to a DownloadItem that exists.
  bool IsValid() const { return ((manager_ != NULL) && (local_id_ >= 0)); }

  // The following methods (operator==, hash(), copy, and assign) provide
  // support for STL containers such as hash_map.

  bool operator==(const DownloadId& that) const {
    return ((that.local_id_ == local_id_) &&
            (that.manager_ == manager_));
  }
  bool operator<(const DownloadId& that) const {
    // Even though DownloadManager* < DownloadManager* is not well defined and
    // GCC does not require it for hash_map, MSVC requires operator< for
    // hash_map. We don't ifdef it out here because we will probably make a
    // set<DownloadId> at some point, when GCC will require it.
    return ((manager_ < that.manager_) ||
            ((manager_ == that.manager_) && (local_id_ < that.local_id_)));
  }

  size_t hash() const {
    // The top half of manager is unlikely to be distinct, and the user is
    // unlikely to have >64K downloads. If these assumptions are incorrect, then
    // DownloadFileManager's hash_map might have a few collisions, but it will
    // use operator== to safely disambiguate.
    return reinterpret_cast<size_t>(manager_) +
           (static_cast<size_t>(local_id_) << (4 * sizeof(size_t)));
  }

 private:
  // DownloadId is used mostly off the UI thread, so manager's methods can't be
  // called, but the pointer can be compared.
  const DownloadManager* manager_;

  int32 local_id_;

  friend CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                                 const DownloadId& global_id);

  // Allow copy and assign.
};

// Allow logging DownloadIds. Looks like "0x01234567:42".
CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const DownloadId& global_id);

// Allow using DownloadIds as keys in hash_maps.
namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_GCC)
template<> struct hash<DownloadId> {
  std::size_t operator()(const DownloadId& id) const {
    return id.hash();
  }
};
#elif defined(COMPILER_MSVC)
inline size_t hash_value(const DownloadId& id) {
  return id.hash();
}
#endif  // COMPILER
}
#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ID_H_
