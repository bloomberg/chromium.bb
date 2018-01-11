// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_H_

#include <inttypes.h>

#include <iosfwd>
#include <string>
#include <tuple>

#include "base/hash.h"
#include "base/unguessable_token.h"
#include "components/viz/common/viz_common_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace viz {
namespace mojom {
class LocalSurfaceIdDataView;
}

class VIZ_COMMON_EXPORT LocalSurfaceId {
 public:
  constexpr LocalSurfaceId()
      : parent_sequence_number_(0), child_sequence_number_(0) {}

  constexpr LocalSurfaceId(const LocalSurfaceId& other)
      : parent_sequence_number_(other.parent_sequence_number_),
        child_sequence_number_(other.child_sequence_number_),
        nonce_(other.nonce_) {}

  constexpr LocalSurfaceId(uint32_t parent_sequence_number,
                           const base::UnguessableToken& nonce)
      : parent_sequence_number_(parent_sequence_number),
        child_sequence_number_(1),
        nonce_(nonce) {}

  constexpr LocalSurfaceId(uint32_t parent_sequence_number,
                           uint32_t child_sequence_number,
                           const base::UnguessableToken& nonce)
      : parent_sequence_number_(parent_sequence_number),
        child_sequence_number_(child_sequence_number),
        nonce_(nonce) {}

  constexpr bool is_valid() const {
    return parent_sequence_number_ != 0 && child_sequence_number_ != 0 &&
           !nonce_.is_empty();
  }

  constexpr uint32_t parent_sequence_number() const {
    return parent_sequence_number_;
  }

  constexpr uint32_t child_sequence_number() const {
    return child_sequence_number_;
  }

  constexpr const base::UnguessableToken& nonce() const { return nonce_; }

  bool operator==(const LocalSurfaceId& other) const {
    return parent_sequence_number_ == other.parent_sequence_number_ &&
           child_sequence_number_ == other.child_sequence_number_ &&
           nonce_ == other.nonce_;
  }

  bool operator!=(const LocalSurfaceId& other) const {
    return !(*this == other);
  }

  size_t hash() const {
    DCHECK(is_valid()) << ToString();
    return base::HashInts(
        static_cast<uint64_t>(
            base::HashInts(parent_sequence_number_, child_sequence_number_)),
        static_cast<uint64_t>(base::UnguessableTokenHash()(nonce_)));
  }

  std::string ToString() const;

 private:
  friend struct mojo::StructTraits<mojom::LocalSurfaceIdDataView,
                                   LocalSurfaceId>;

  friend bool operator<(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs);

  uint32_t parent_sequence_number_;
  uint32_t child_sequence_number_;
  base::UnguessableToken nonce_;
};

VIZ_COMMON_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const LocalSurfaceId& local_surface_id);

inline bool operator<(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return std::tie(lhs.parent_sequence_number_, lhs.child_sequence_number_,
                  lhs.nonce_) < std::tie(rhs.parent_sequence_number_,
                                         rhs.child_sequence_number_,
                                         rhs.nonce_);
}

inline bool operator>(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return operator<(rhs, lhs);
}

inline bool operator<=(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return !operator>(lhs, rhs);
}

inline bool operator>=(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return !operator<(lhs, rhs);
}

struct LocalSurfaceIdHash {
  size_t operator()(const LocalSurfaceId& key) const { return key.hash(); }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_H_
