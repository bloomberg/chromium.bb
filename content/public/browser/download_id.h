// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_ID_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_ID_H_

#include <iosfwd>
#include <string>

#include "base/hash_tables.h"
#include "base/stringprintf.h"
#include "content/common/content_export.h"

namespace content {

// DownloadId combines per-profile Download ids with an indication of which
// profile in order to be globally unique. DownloadIds are not persistent across
// sessions, but their local() field is.
class DownloadId {
 public:
  static DownloadId Invalid() { return DownloadId(); }

  // Domain separates spaces of local ids.
  typedef const void* Domain;

  DownloadId() : domain_(NULL), local_id_(-1) {}

  DownloadId(Domain domain, int32 local_id)
    : domain_(domain),
      local_id_(local_id) {
  }

  // Return the per-profile and persistent part of this DownloadId.
  int32 local() const { return local_id_; }

  // Returns true if this DownloadId has been allocated and could possibly refer
  // to a DownloadItem that exists.
  bool IsValid() const { return ((domain_ != NULL) && (local_id_ >= 0)); }

  // The following methods (operator==, hash(), copy, and assign) provide
  // support for STL containers such as hash_map.

  bool operator==(const DownloadId& that) const {
    return ((that.local_id_ == local_id_) &&
            (that.domain_ == domain_));
  }
  bool operator<(const DownloadId& that) const {
    // Even though Domain::operator< is not well defined and
    // GCC does not require it for hash_map, MSVC requires operator< for
    // hash_map. We don't ifdef it out here because we will probably make a
    // set<DownloadId> at some point, when GCC will require it.
    return ((domain_ < that.domain_) ||
            ((domain_ == that.domain_) && (local_id_ < that.local_id_)));
  }

  size_t hash() const {
    // The top half of domain_ is unlikely to be distinct, and the user is
    // unlikely to have >64K downloads. If these assumptions are incorrect, then
    // hash_maps containing DownloadIds might have a few collisions, but can
    // use operator== to safely disambiguate.
    return reinterpret_cast<size_t>(domain_) +
           (static_cast<size_t>(local_id_) << (4 * sizeof(size_t)));
  }

  std::string DebugString() const {
    return base::StringPrintf("%p:%d", domain_, local_id_);
  }

 private:
  Domain domain_;

  int32 local_id_;

  // Allow copy and assign.
};

}  // namespace content

// Allow logging DownloadIds. Looks like "0x01234567:42".
CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const content::DownloadId& global_id);
inline std::ostream& operator<<(std::ostream& out,
                                const content::DownloadId& global_id) {
  return out << global_id.DebugString();
}

// Allow using DownloadIds as keys in hash_maps.
namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_GCC)
template<> struct hash<content::DownloadId> {
  std::size_t operator()(const content::DownloadId& id) const {
    return id.hash();
  }
};
#elif defined(COMPILER_MSVC)
inline size_t hash_value(const content::DownloadId& id) {
  return id.hash();
}
#endif  // COMPILER
}
#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_ID_H_
