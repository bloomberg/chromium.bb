// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_COMMON_CERT_SET_H_
#define NET_QUIC_CRYPTO_COMMON_CERT_SET_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_protocol.h"

namespace net {

// CommonCertSet is an interface to an object that contains a number of common
// certificate sets and can match against them.
class NET_EXPORT_PRIVATE CommonCertSet {
 public:
  virtual ~CommonCertSet();

  // GetCommonHashes returns a StringPiece containing the hashes of common sets
  // supported by this object.
  virtual base::StringPiece GetCommonHashes() = 0;

  // GetCert returns a specific certificate in the common set identified by
  // |hash|. If no such certificate is known, an empty StringPiece is returned.
  virtual base::StringPiece GetCert(uint64 hash, uint32 index) = 0;

  // MatchCert tries to find |cert| in one of the common certificate sets
  // identified by |common_set_hashes|. On success it puts the hash in
  // |out_hash|, the index in the set in |out_index| and returns true. Otherwise
  // it returns false.
  virtual bool MatchCert(base::StringPiece cert,
                         base::StringPiece common_set_hashes,
                         uint64* out_hash,
                         uint32* out_index) = 0;
};

// CommonCertSetQUIC implements the CommonCertSet interface using the default
// certificate sets.
class NET_EXPORT_PRIVATE CommonCertSetQUIC : public CommonCertSet {
 public:
  CommonCertSetQUIC();

  // CommonCertSet interface.
  virtual base::StringPiece GetCommonHashes() OVERRIDE;

  virtual base::StringPiece GetCert(uint64 hash, uint32 index) OVERRIDE;

  virtual bool MatchCert(base::StringPiece cert,
                         base::StringPiece common_set_hashes,
                         uint64* out_hash,
                         uint32* out_index) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommonCertSetQUIC);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_COMMON_CERT_SET_H_
