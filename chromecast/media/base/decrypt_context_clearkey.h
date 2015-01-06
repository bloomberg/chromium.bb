// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_CLEARKEY_H_
#define CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_CLEARKEY_H_

#include "base/macros.h"
#include "chromecast/media/base/decrypt_context.h"

namespace chromecast {
namespace media {

class DecryptContextClearKey : public DecryptContext {
 public:
  // Note: DecryptContextClearKey does not take ownership of |key|.
  explicit DecryptContextClearKey(crypto::SymmetricKey* key);

  // DecryptContext implementation.
  crypto::SymmetricKey* GetKey() const override;

 private:
  ~DecryptContextClearKey() override;

  crypto::SymmetricKey* const key_;

  DISALLOW_COPY_AND_ASSIGN(DecryptContextClearKey);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_CLEARKEY_H_
