// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_URL_REQUEST_STRUCT_TRAITS_H_
#define CONTENT_COMMON_URL_REQUEST_STRUCT_TRAITS_H_

#include "content/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/request_priority.h"

namespace mojo {

template <>
struct EnumTraits<content::mojom::RequestPriority, net::RequestPriority> {
  static content::mojom::RequestPriority ToMojom(net::RequestPriority priority);
  static bool FromMojom(content::mojom::RequestPriority in,
                        net::RequestPriority* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_URL_REQUEST_STRUCT_TRAITS_H_
