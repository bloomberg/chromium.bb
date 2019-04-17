// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_ENCODING_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_ENCODING_H_

#include <string>

#include "third_party/inspector_protocol/encoding/encoding.h"

namespace content {

// Whether --enable-internal-devtools-binary-parotocol was passed on the command
// line.  If so, the DevtoolsSession will convert all outgoing traffic to agents
// / handlers / etc. to the CBOR-based binary protocol.
bool EnableInternalDevToolsBinaryProtocol();

// Conversion routines between the inspector protocol binary wire format
// (based on CBOR RFC 7049) and JSON.
std::string ConvertCBORToJSON(inspector_protocol_encoding::span<uint8_t> cbor);
std::string ConvertJSONToCBOR(inspector_protocol_encoding::span<uint8_t> json);
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_ENCODING_H_
