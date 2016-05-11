// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_
#define CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/base/key_systems_common.h"
#include "chromecast/public/media/decrypt_context.h"

namespace chromecast {
namespace media {

// Base class of a decryption context: a decryption context gathers all the
// information needed to decrypt frames with a given key id.
// Each CDM should implement this and add fields needed to fully describe a
// decryption context.
class DecryptContextImpl : public DecryptContext {
 public:
  explicit DecryptContextImpl(CastKeySystem key_system);
  ~DecryptContextImpl() override;

  // DecryptContext implementation:
  CastKeySystem GetKeySystem() override;
  bool Decrypt(CastDecoderBuffer* buffer,
               std::vector<uint8_t>* output) final;

  // TODO(yucliu): replace DecryptContext::Decrypt with this one in next
  // public api releasing.
  // Decrypts the given buffer. Returns true/false for success/failure,
  // and places the decrypted data in |output| if successful.
  // Decrypted data in |output| has the same length as |buffer|.
  virtual bool Decrypt(CastDecoderBuffer* buffer, uint8_t* output) = 0;

  // Returns whether the data can be decrypted into user memory.
  // If the key system doesn't support secure output or the app explicitly
  // requires non secure output, it should return true;
  // If the key system doesn't allow clear content to be decrypted into user
  // memory, it should return false.
  virtual bool CanDecryptToBuffer() const;

 private:
  CastKeySystem key_system_;

  DISALLOW_COPY_AND_ASSIGN(DecryptContextImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_
