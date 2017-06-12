// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MUTABLE_NETWORK_TRAFFIC_ANNOTATION_PARAM_TRAITS_H_
#define CONTENT_COMMON_MUTABLE_NETWORK_TRAFFIC_ANNOTATION_PARAM_TRAITS_H_

#include "content/common/mutable_network_traffic_annotation_tag.mojom.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::MutableNetworkTrafficAnnotationTagDataView,
                    net::MutableNetworkTrafficAnnotationTag> {
  static int32_t unique_id_hash_code(
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    return traffic_annotation.unique_id_hash_code;
  }
  static bool Read(
      content::mojom::MutableNetworkTrafficAnnotationTagDataView data,
      net::MutableNetworkTrafficAnnotationTag* out) {
    out->unique_id_hash_code = data.unique_id_hash_code();
    return true;
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_MUTABLE_NETWORK_TRAFFIC_ANNOTATION_PARAM_TRAITS_H_
