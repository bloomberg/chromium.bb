// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/local_surface_id.h"

#include "base/strings/stringprintf.h"

namespace viz {

bool LocalSurfaceId::ParentComponent::operator==(
    const ParentComponent& other) const {
  return sequence_number == other.sequence_number &&
         embed_token == other.embed_token;
}

bool LocalSurfaceId::ParentComponent::operator!=(
    const ParentComponent& other) const {
  return !(*this == other);
}

bool LocalSurfaceId::ParentComponent::IsNewerThan(
    const ParentComponent& other) const {
  return embed_token == other.embed_token &&
         sequence_number > other.sequence_number;
}

bool LocalSurfaceId::ParentComponent::IsSameOrNewerThan(
    const ParentComponent& other) const {
  return embed_token == other.embed_token &&
         sequence_number >= other.sequence_number;
}

std::string LocalSurfaceId::ToString() const {
  std::string embed_token =
      VLOG_IS_ON(1)
          ? parent_component_.embed_token.ToString()
          : parent_component_.embed_token.ToString().substr(0, 4) + "...";

  return base::StringPrintf("LocalSurfaceId(%u, %u, %s)",
                            parent_component_.sequence_number,
                            child_sequence_number_, embed_token.c_str());
}

std::ostream& operator<<(std::ostream& out,
                         const LocalSurfaceId& local_surface_id) {
  return out << local_surface_id.ToString();
}

bool LocalSurfaceId::IsNewerThan(const LocalSurfaceId& other) const {
  return (parent_component_.IsNewerThan(other.parent_component_) &&
          child_sequence_number_ >= other.child_sequence_number_) ||
         (parent_component_ == other.parent_component_ &&
          child_sequence_number_ > other.child_sequence_number_);
}

bool LocalSurfaceId::IsSameOrNewerThan(const LocalSurfaceId& other) const {
  return IsNewerThan(other) || *this == other;
}

LocalSurfaceId LocalSurfaceId::ToSmallestId() const {
  return LocalSurfaceId(1, 1, embed_token());
}

}  // namespace viz
