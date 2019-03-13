// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_ENCODING_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_ENCODING_H_

#include <string>

namespace content {

// Whether --enable-internal-devtools-binary-parotocol was passed on the command
// line.  If so, the DevtoolsSession will convert all outgoing traffic to agents
// / handlers / etc. to the CBOR-based binary protocol.
bool EnableInternalDevToolsBinaryProtocol();

// Conversion routines between the inspector protocol binary wire format
// (based on CBOR RFC 7049) and JSON.
std::string ConvertCBORToJSON(const std::string& cbor);
std::string ConvertJSONToCBOR(const std::string& json);

// Whether |serialized| is CBOR produced by the inspector protocol.
// We always enclose messages with an envelope, that is, the 0xd8 tag
// followed by the indicator for the byte string, followed by a 32 bit
// length value (4 bytes).
bool IsCBOR(const std::string& serialized);
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_ENCODING_H_
