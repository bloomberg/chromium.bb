// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_LOCAL_SURFACE_ID_H_
#define CC_SURFACES_LOCAL_SURFACE_ID_H_

#include <inttypes.h>

#include <iosfwd>
#include <string>
#include <tuple>

#include "base/hash.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace cc {
namespace mojom {
class LocalSurfaceIdDataView;
}

class LocalSurfaceId {
 public:
  constexpr LocalSurfaceId() : local_id_(0) {}

  constexpr LocalSurfaceId(const LocalSurfaceId& other)
      : local_id_(other.local_id_), nonce_(other.nonce_) {}

  constexpr LocalSurfaceId(uint32_t local_id,
                           const base::UnguessableToken& nonce)
      : local_id_(local_id), nonce_(nonce) {}

  constexpr bool is_valid() const {
    return local_id_ != 0 && !nonce_.is_empty();
  }

  constexpr uint32_t local_id() const { return local_id_; }

  constexpr const base::UnguessableToken& nonce() const { return nonce_; }

  bool operator==(const LocalSurfaceId& other) const {
    return local_id_ == other.local_id_ && nonce_ == other.nonce_;
  }

  bool operator!=(const LocalSurfaceId& other) const {
    return !(*this == other);
  }

  bool operator<(const LocalSurfaceId& other) const {
    return std::tie(local_id_, nonce_) <
           std::tie(other.local_id_, other.nonce_);
  }

  size_t hash() const {
    DCHECK(is_valid()) << ToString();
    return base::HashInts(
        local_id_, static_cast<uint64_t>(base::UnguessableTokenHash()(nonce_)));
  }

  std::string ToString() const;

 private:
  friend struct mojo::StructTraits<mojom::LocalSurfaceIdDataView,
                                   LocalSurfaceId>;

  uint32_t local_id_;
  base::UnguessableToken nonce_;
};

std::ostream& operator<<(std::ostream& out,
                         const LocalSurfaceId& local_surface_id);

struct LocalSurfaceIdHash {
  size_t operator()(const LocalSurfaceId& key) const { return key.hash(); }
};

}  // namespace cc

#endif  // CC_SURFACES_LOCAL_SURFACE_ID_H_
