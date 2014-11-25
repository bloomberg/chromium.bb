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
#if defined(ENABLE_BROWSER_CDMS)
  static const int kInvalidCdmId = 0;
#endif

  virtual ~CdmContext();

  // Gets the Decryptor object associated with the CDM. Returns NULL if no
  // Decryptor object is associated. The returned object is only guaranteed
  // to be valid during the CDM's lifetime.
  virtual Decryptor* GetDecryptor() = 0;

#if defined(ENABLE_BROWSER_CDMS)
  // Returns the CDM ID associated with |this|. May be kInvalidCdmId if no CDM
  // ID is associated.
  virtual int GetCdmId() const = 0;
#endif

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
