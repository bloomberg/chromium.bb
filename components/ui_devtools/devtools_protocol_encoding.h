// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
#define COMPONENTS_UI_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_

#include "third_party/inspector_protocol/encoding/encoding.h"

// Convenience adaptation of the conversion function
// ::inspector_protocol_encoding::json::ConvertCBORToJSON,
// using an implementation of
// ::inspector_protocol_encoding::json::Platform that
// delegates to base/strings/string_number_conversions.h.
namespace ui_devtools {
::inspector_protocol_encoding::Status ConvertCBORToJSON(
    ::inspector_protocol_encoding::span<uint8_t> cbor,
    std::string* json);
}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
