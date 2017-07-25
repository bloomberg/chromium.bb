// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_STREAM_TYPEMAP_TRAITS_H_
#define CONTENT_COMMON_MEDIA_MEDIA_STREAM_TYPEMAP_TRAITS_H_

#include "content/common/media/media_stream.mojom.h"
#include "content/public/common/media_stream_request.h"

namespace mojo {

template <>
struct EnumTraits<content::mojom::MediaStreamType, content::MediaStreamType> {
  static content::mojom::MediaStreamType ToMojom(content::MediaStreamType type);

  static bool FromMojom(content::mojom::MediaStreamType input,
                        content::MediaStreamType* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_MEDIA_MEDIA_STREAM_TYPEMAP_TRAITS_H_