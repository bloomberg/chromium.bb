// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_CONTEXT_H_
#define MEDIA_BASE_CDM_CONTEXT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

class Decryptor;

// An interface representing the context that a media pipeline needs from a
// content decryption module (CDM) to decrypt (and decode) encrypted buffers.
class MEDIA_EXPORT CdmContext {
 public:
  // Indicates an invalid CDM ID. See GetCdmId() for details.
  static const int kInvalidCdmId = 0;

  virtual ~CdmContext();

  // Gets the Decryptor object associated with the CDM. Returns NULL if the CDM
  // does not support a Decryptor. The returned object is only guaranteed to be
  // valid during the CDM's lifetime.
  virtual Decryptor* GetDecryptor() = 0;

  // Returns an ID associated with the CDM, which can be used to locate the real
  // CDM instance. This is useful when the CDM is hosted remotely, e.g. in a
  // different process.
  // Returns kInvalidCdmId if the CDM cannot be used remotely. In this case,
  // GetDecryptor() should return a non-null Decryptor.
  virtual int GetCdmId() const = 0;

 protected:
  CdmContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmContext);
};

// Callback to notify that the CdmContext has been completely attached to
// the media pipeline. Parameter indicates whether the operation succeeded.
typedef base::Callback<void(bool)> CdmAttachedCB;

// A dummy implementation of CdmAttachedCB.
MEDIA_EXPORT void IgnoreCdmAttached(bool success);

}  // namespace media

#endif  // MEDIA_BASE_CDM_CONTEXT_H_
