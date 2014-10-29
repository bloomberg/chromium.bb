// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_ICA_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_ICA_H_

#include <stddef.h>

#include "base/strings/string_piece.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

// Gets the trusted ICA entry for the cert represented by |data|.
// Returns the serialized certificate as bytes if the ICA was found.
// Returns an empty-length StringPiece if the ICA was not found.
base::StringPiece GetTrustedICAPublicKey(const base::StringPiece& data);

// Gets the default trusted ICA for legacy compatibility.
base::StringPiece GetDefaultTrustedICAPublicKey();

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_ICA_H_
