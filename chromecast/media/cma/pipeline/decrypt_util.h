// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_

#include "base/memory/ref_counted.h"

namespace crypto {
class SymmetricKey;
}

namespace chromecast {
namespace media {

class DecoderBufferBase;

// Create a new buffer which corresponds to the clear version of |buffer|.
// Note: the memory area corresponding to the ES data of the new buffer
// is the same as the ES data of |buffer| (for efficiency).
// After the function is called, |buffer| is left in a inconsistent state
// in the sense it has some decryption info but the ES data is now in clear.
scoped_refptr<DecoderBufferBase> DecryptDecoderBuffer(
    const scoped_refptr<DecoderBufferBase>& buffer,
    crypto::SymmetricKey* key);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_
