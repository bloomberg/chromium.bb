// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_custom_types_struct_traits.h"

#include <iterator>

#include "base/version.h"

namespace mojo {

// static
const std::vector<uint32_t>&
StructTraits<mojo::common::mojom::VersionDataView, base::Version>::components(
    const base::Version& version) {
  return version.components();
}

// static
bool StructTraits<mojo::common::mojom::VersionDataView, base::Version>::Read(
    mojo::common::mojom::VersionDataView data,
    base::Version* out) {
  std::vector<uint32_t> components;
  if (!data.ReadComponents(&components))
    return false;

  *out = base::Version(base::Version(std::move(components)));
  return out->IsValid();
}

}  // namespace mojo
