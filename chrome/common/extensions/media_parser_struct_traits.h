// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MEDIA_PARSER_STRUCT_TRAITS_H_
#define CHROME_COMMON_EXTENSIONS_MEDIA_PARSER_STRUCT_TRAITS_H_

#include <string>

#include "chrome/common/extensions/media_parser.mojom.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "mojo/public/cpp/bindings/array_traits_carray.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::AttachedImageDataView,
                    ::metadata::AttachedImage> {
  static const std::string& type(const ::metadata::AttachedImage& image) {
    return image.type;
  }

  static ConstCArray<uint8_t> data(const ::metadata::AttachedImage& image) {
    // TODO(dcheng): perhaps metadata::AttachedImage should consider passing the
    // image data around in a std::vector<uint8_t>.
    return ConstCArray<uint8_t>(
        image.data.size(), reinterpret_cast<const uint8_t*>(image.data.data()));
  }

  static bool Read(extensions::mojom::AttachedImageDataView view,
                   ::metadata::AttachedImage* out);
};

}  // namespace mojo

#endif  // CHROME_COMMON_EXTENSIONS_MEDIA_PARSER_STRUCT_TRAITS_H_
