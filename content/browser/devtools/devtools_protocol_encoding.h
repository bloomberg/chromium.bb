// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_

#include <memory>
#include "third_party/inspector_protocol/encoding/encoding.h"

// Convenience adaptations of the conversion functions
// ::inspector_protocol_encoding::json::ConvertCBORToJSON
// ::inspector_protocol_encoding::json::ConvertJDONToCBOR.
// These adaptations use an implementation of
// ::inspector_protocol_encoding::json::Platform that
// uses base/strings/string_number_conversions.h.
namespace content {

::inspector_protocol_encoding::Status ConvertCBORToJSON(
    ::inspector_protocol_encoding::span<uint8_t> cbor,
    std::string* json);

::inspector_protocol_encoding::Status ConvertJSONToCBOR(
    ::inspector_protocol_encoding::span<uint8_t> json,
    std::string* cbor);

::inspector_protocol_encoding::Status ConvertJSONToCBOR(
    ::inspector_protocol_encoding::span<uint8_t> json,
    std::vector<uint8_t>* cbor);
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
