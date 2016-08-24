// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_COMMON_CUSTOM_TYPES_STRUCT_TRAITS_H_
#define MOJO_COMMON_COMMON_CUSTOM_TYPES_STRUCT_TRAITS_H_

#include "mojo/common/common_custom_types.mojom.h"

namespace base {
class Version;
}

namespace mojo {

template <>
struct StructTraits<mojo::common::mojom::VersionDataView, base::Version> {
  static bool IsNull(const base::Version& version) {
    return !version.IsValid();
  }
  static void SetToNull(base::Version* out) {
    *out = base::Version(std::string());
  }
  static const std::vector<uint32_t>& components(const base::Version& version);
  static bool Read(mojo::common::mojom::VersionDataView data,
                   base::Version* out);
};

}  // namespace mojo

#endif  // MOJO_COMMON_COMMON_CUSTOM_TYPES_STRUCT_TRAITS_H_
