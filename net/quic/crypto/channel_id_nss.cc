// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/channel_id.h"

using base::StringPiece;

namespace net {

// TODO(rtenneti): Implement NSS support for ChannelIDVerifier::Verify and
// real ChannelIDSigner (using the SpdyCredentialBuilder as a model).
// static
bool ChannelIDVerifier::Verify(StringPiece key,
                               StringPiece signed_data,
                               StringPiece signature) {
  return true;
}

}  // namespace net
