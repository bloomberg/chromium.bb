// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BINDER_CONSTANTS_H_
#define BINDER_CONSTANTS_H_

#define BINDER_PACK_CHARS(c1, c2, c3, c4) \
  (((c1) << 24) | ((c2) << 16) | ((c3) << 8) | (c4))

namespace binder {

// Context manager's handle is always 0.
const uint32 kContextManagerHandle = 0;

// Transaction code constants.
const uint32 kFirstTransactionCode = 0x00000001;
const uint32 kLastTransactionCode = 0x00ffffff;
const uint32 kPingTransactionCode = BINDER_PACK_CHARS('_', 'P', 'N', 'G');

}  // namespace binder

#endif  // BINDER_CONSTANTS_H_
