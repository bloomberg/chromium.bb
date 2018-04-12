// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic crypto

#ifndef NET_QUIC_CORE_CRYPTO_CRYPTO_UTILS_H_
#define NET_QUIC_CORE_CRYPTO_CRYPTO_UTILS_H_

#include <cstddef>
#include <cstdint>

#include "base/macros.h"
#include "net/quic/core/crypto/crypto_handshake.h"
#include "net/quic/core/crypto/crypto_handshake_message.h"
#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/core/quic_packets.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/platform/api/quic_export.h"
#include "net/quic/platform/api/quic_string.h"
#include "net/quic/platform/api/quic_string_piece.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

class QuicRandom;

class QUIC_EXPORT_PRIVATE CryptoUtils {
 public:
  // Diversification is a utility class that's used to act like a union type.
  // Values can be created by calling the functions like |NoDiversification|,
  // below.
  class Diversification {
   public:
    enum Mode {
      NEVER,  // Key diversification will never be used. Forward secure
              // crypters will always use this mode.

      PENDING,  // Key diversification will happen when a nonce is later
                // received. This should only be used by clients initial
                // decrypters which are waiting on the divesification nonce
                // from the server.

      NOW,  // Key diversification will happen immediate based on the nonce.
            // This should only be used by servers initial encrypters.
    };

    Diversification(const Diversification& diversification) = default;

    static Diversification Never() { return Diversification(NEVER, nullptr); }
    static Diversification Pending() {
      return Diversification(PENDING, nullptr);
    }
    static Diversification Now(DiversificationNonce* nonce) {
      return Diversification(NOW, nonce);
    }

    Mode mode() const { return mode_; }
    DiversificationNonce* nonce() const {
      DCHECK_EQ(mode_, NOW);
      return nonce_;
    }

   private:
    Diversification(Mode mode, DiversificationNonce* nonce)
        : mode_(mode), nonce_(nonce) {}

    Mode mode_;
    DiversificationNonce* nonce_;
  };

  // Implements the QHKDF-Expand function defined in section 5.2.3 of
  // draft-ietf-quic-tls-09. The QHKDF-Expand function takes 3 explicit
  // arguments, as well as an implicit PRF which is the hash function negotiated
  // by TLS. This PRF is passed in as |prf|; the explicit arguments to
  // QHKDF-Expand - Secret, Label, and Length - are passed in as |secret|,
  // |label|, and |out_len|, respectively.
  static std::vector<uint8_t> QhkdfExpand(const EVP_MD* prf,
                                          const std::vector<uint8_t>& secret,
                                          const std::string& label,
                                          size_t out_len);

  // SetKeyAndIV derives the key and IV from the given packet protection secret
  // |pp_secret| and sets those fields on the given QuicEncrypter or
  // QuicDecrypter |*crypter|. This follows the derivation described in section
  // 5.2.4 of draft-ietf-quic-tls-09.
  template <class QuicCrypter>
  static void SetKeyAndIV(const EVP_MD* prf,
                          const std::vector<uint8_t>& pp_secret,
                          QuicCrypter* crypter);

  // QUIC encrypts TLS handshake messages with a version-specific key (to
  // prevent network observers that are not aware of that QUIC version from
  // making decisions based on the TLS handshake). This packet protection secret
  // is derived from the connection ID in the client's Initial packet.
  //
  // This function takes that |connection_id| and creates the encrypter and
  // decrypter (put in |*crypters|) to use for this packet protection, as well
  // as setting the key and IV on those crypters.
  static void CreateTlsInitialCrypters(Perspective perspective,
                                       QuicConnectionId connection_id,
                                       CrypterPair* crypters);

  // Generates the connection nonce. The nonce is formed as:
  //   <4 bytes> current time
  //   <8 bytes> |orbit| (or random if |orbit| is empty)
  //   <20 bytes> random
  static void GenerateNonce(QuicWallTime now,
                            QuicRandom* random_generator,
                            QuicStringPiece orbit,
                            QuicString* nonce);

  // DeriveKeys populates |crypters->encrypter|, |crypters->decrypter|, and
  // |subkey_secret| (optional -- may be null) given the contents of
  // |premaster_secret|, |client_nonce|, |server_nonce| and |hkdf_input|. |aead|
  // determines which cipher will be used. |perspective| controls whether the
  // server's keys are assigned to |encrypter| or |decrypter|. |server_nonce| is
  // optional and, if non-empty, is mixed into the key derivation.
  // |subkey_secret| will have the same length as |premaster_secret|.
  //
  // If the mode of |diversification| is NEVER, the the crypters will be
  // configured to never perform key diversification. If the mode is
  // NOW (which is only for servers, then the encrypter will be keyed via a
  // two-step process that uses the nonce from |diversification|.
  // If the mode is PENDING (which is only for servres), then the
  // decrypter will only be keyed to a preliminary state: a call to
  // |SetDiversificationNonce| with a diversification nonce will be needed to
  // complete keying.
  static bool DeriveKeys(QuicStringPiece premaster_secret,
                         QuicTag aead,
                         QuicStringPiece client_nonce,
                         QuicStringPiece server_nonce,
                         const QuicString& hkdf_input,
                         Perspective perspective,
                         Diversification diversification,
                         CrypterPair* crypters,
                         QuicString* subkey_secret);

  // Performs key extraction to derive a new secret of |result_len| bytes
  // dependent on |subkey_secret|, |label|, and |context|. Returns false if the
  // parameters are invalid (e.g. |label| contains null bytes); returns true on
  // success.
  static bool ExportKeyingMaterial(QuicStringPiece subkey_secret,
                                   QuicStringPiece label,
                                   QuicStringPiece context,
                                   size_t result_len,
                                   QuicString* result);

  // Computes the FNV-1a hash of the provided DER-encoded cert for use in the
  // XLCT tag.
  static uint64_t ComputeLeafCertHash(QuicStringPiece cert);

  // Validates that |server_hello| is actually an SHLO message and that it is
  // not part of a downgrade attack.
  //
  // Returns QUIC_NO_ERROR if this is the case or returns the appropriate error
  // code and sets |error_details|.
  static QuicErrorCode ValidateServerHello(
      const CryptoHandshakeMessage& server_hello,
      const QuicTransportVersionVector& negotiated_versions,
      QuicString* error_details);

  // Validates that |client_hello| is actually a CHLO and that this is not part
  // of a downgrade attack.
  // This includes verifiying versions and detecting downgrade attacks.
  //
  // Returns QUIC_NO_ERROR if this is the case or returns the appropriate error
  // code and sets |error_details|.
  static QuicErrorCode ValidateClientHello(
      const CryptoHandshakeMessage& client_hello,
      QuicTransportVersion version,
      const QuicTransportVersionVector& supported_versions,
      QuicString* error_details);

  // Returns the name of the HandshakeFailureReason as a char*
  static const char* HandshakeFailureReasonToString(
      HandshakeFailureReason reason);

  // Writes a hash of the serialized |message| into |output|.
  static void HashHandshakeMessage(const CryptoHandshakeMessage& message,
                                   QuicString* output,
                                   Perspective perspective);

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptoUtils);
};

}  // namespace net

#endif  // NET_QUIC_CORE_CRYPTO_CRYPTO_UTILS_H_
