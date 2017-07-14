// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_REFERENCE_H_
#define CC_SURFACES_SURFACE_REFERENCE_H_

#include <string>

#include "base/hash.h"
#include "cc/surfaces/surfaces_export.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace cc {

// Hold a reference from an embedding (parent) to embedded (child) surface.
class CC_SURFACES_EXPORT SurfaceReference {
 public:
  SurfaceReference();
  SurfaceReference(const viz::SurfaceId& parent_id,
                   const viz::SurfaceId& child_id);
  SurfaceReference(const SurfaceReference& other);

  ~SurfaceReference();

  const viz::SurfaceId& parent_id() const { return parent_id_; }
  const viz::SurfaceId& child_id() const { return child_id_; }

  size_t hash() const {
    return base::HashInts(static_cast<uint64_t>(parent_id_.hash()),
                          static_cast<uint64_t>(child_id_.hash()));
  }

  bool operator==(const SurfaceReference& other) const {
    return parent_id_ == other.parent_id_ && child_id_ == other.child_id_;
  }

  bool operator!=(const SurfaceReference& other) const {
    return !(*this == other);
  }

  bool operator<(const SurfaceReference& other) const {
    return std::tie(parent_id_, child_id_) <
           std::tie(other.parent_id_, other.child_id_);
  }

  std::string ToString() const;

 private:
  viz::SurfaceId parent_id_;
  viz::SurfaceId child_id_;
};

struct SurfaceReferenceHash {
  size_t operator()(const SurfaceReference& ref) const { return ref.hash(); }
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_REFERENCE_H_
