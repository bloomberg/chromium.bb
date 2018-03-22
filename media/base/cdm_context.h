// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_CONTEXT_H_
#define MEDIA_BASE_CDM_CONTEXT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace media {

class CdmProxyContext;
class Decryptor;
class MediaCryptoContext;

// An interface representing the context that a media player needs from a
// content decryption module (CDM) to decrypt (and decode) encrypted buffers.
// Typically this will be passed to the media player (e.g. using SetCdm()).
class MEDIA_EXPORT CdmContext {
 public:
  // Indicates an invalid CDM ID. See GetCdmId() for details.
  enum { kInvalidCdmId = 0 };

  virtual ~CdmContext();

  // Gets the Decryptor object associated with the CDM. Returns nullptr if the
  // CDM does not support a Decryptor (i.e. platform-based CDMs where decryption
  // occurs implicitly along with decoding). The returned object is only
  // guaranteed to be valid during the CDM's lifetime.
  virtual Decryptor* GetDecryptor();

  // Returns an ID that can be used to find a remote CDM, in which case this CDM
  // serves as a proxy to the remote one. Returns kInvalidCdmId when remote CDM
  // is not supported (e.g. this CDM is a local CDM).
  virtual int GetCdmId() const;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Returns a CdmProxyContext. The default implementation returns a nullptr
  // to indidate that there is no context.
  // If CdmProxy is not used, then this returns a nullptr.
  // The pointer is owned by the callee.
  virtual CdmProxyContext* GetCdmProxyContext();
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_ANDROID)
  // Returns a MediaCryptoContext that can be used by MediaCodec based decoders.
  // The returned object is only guaranteed to be valid during the CDM's
  // lifetime.
  virtual MediaCryptoContext* GetMediaCryptoContext();
#endif

  // Returns a unique class identifier. Some subclasses override and use this
  // method to provide safe down-casting to their type.
  virtual void* GetClassIdentifier() const;

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

// A reference holder to make sure the CdmContext is always valid as long as
// |this| is alive. Typically |this| will hold a reference (directly or
// indirectly) to the host, e.g. a ContentDecryptionModule or a CdmProxy.
// This class must be held on the same thread where the host lives. The raw
// CdmContext pointer returned by GetCdmContext() may be used on other threads
// if it's supported by the CdmContext implementation.
class MEDIA_EXPORT CdmContextRef {
 public:
  virtual ~CdmContextRef() {}

  // Returns the CdmContext which is guaranteed to be alive as long as |this| is
  // alive. This function should never return nullptr.
  virtual CdmContext* GetCdmContext() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_CONTEXT_H_
