// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_H_
#define CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/base/key_systems_common.h"

namespace crypto {
class SymmetricKey;
}

namespace chromecast {
namespace media {

// Base class of a decryption context: a decryption context gathers all the
// information needed to decrypt frames with a given key id.
// Each CDM should implement this and add fields needed to fully describe a
// decryption context.
//
class DecryptContext : public base::RefCountedThreadSafe<DecryptContext> {
 public:
  explicit DecryptContext(CastKeySystem key_system);

  CastKeySystem key_system() const { return key_system_; }

  // Returns the clear key if available, NULL otherwise.
  virtual crypto::SymmetricKey* GetKey() const;

 protected:
  friend class base::RefCountedThreadSafe<DecryptContext>;
  virtual ~DecryptContext();

 private:
  CastKeySystem key_system_;

  DISALLOW_COPY_AND_ASSIGN(DecryptContext);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_H_