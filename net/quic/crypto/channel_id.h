// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CHANNEL_ID_H_
#define NET_QUIC_CRYPTO_CHANNEL_ID_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// ChannelIDSigner is an abstract interface that implements signing by
// ChannelID keys.
class NET_EXPORT_PRIVATE ChannelIDSigner {
 public:
  virtual ~ChannelIDSigner() { }

  // Sign signs |signed_data| using the ChannelID key for |hostname| and puts
  // the serialized public key into |out_key| and the signature into
  // |out_signature|. It returns true on success.
  virtual bool Sign(const std::string& hostname,
                    base::StringPiece signed_data,
                    std::string* out_key,
                    std::string* out_signature) = 0;

  // GetKeyForHostname returns the ChannelID key that |ChannelIDSigner| will use
  // for the given hostname.
  virtual std::string GetKeyForHostname(const std::string& hostname) = 0;
};

// ChannelIDVerifier verifies ChannelID signatures.
class NET_EXPORT_PRIVATE ChannelIDVerifier {
 public:
  // kContextStr is prepended to the data to be signed in order to ensure that
  // a ChannelID signature cannot be used in a different context. (The
  // terminating NUL byte is inclued.)
  static const char kContextStr[];
  // kClientToServerStr follows kContextStr to specify that the ChannelID is
  // being used in the client to server direction. (The terminating NUL byte is
  // included.)
  static const char kClientToServerStr[];

  // Verify returns true iff |signature| is a valid signature of |signed_data|
  // by |key|.
  static bool Verify(base::StringPiece key,
                     base::StringPiece signed_data,
                     base::StringPiece signature);

  // FOR TESTING ONLY: VerifyRaw returns true iff |signature| is a valid
  // signature of |signed_data| by |key|. |is_channel_id_signature| indicates
  // whether |signature| is a ChannelID signature (with kContextStr prepended
  // to the data to be signed).
  static bool VerifyRaw(base::StringPiece key,
                        base::StringPiece signed_data,
                        base::StringPiece signature,
                        bool is_channel_id_signature);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CHANNEL_ID_H_
