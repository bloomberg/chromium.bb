// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CLIENT_UPDATE_PROTOCOL_ECDSA_H_
#define COMPONENTS_UPDATE_CLIENT_CLIENT_UPDATE_PROTOCOL_ECDSA_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"

// Client Update Protocol v2, or CUP-ECDSA, is used by Google Update (Omaha)
// servers to ensure freshness and authenticity of update checks over HTTP,
// without the overhead of HTTPS -- namely, no PKI, no guarantee of privacy,
// and no request replay protection (since update checks are idempotent).
//
// CUP-ECDSA relies on a single signing operation using ECDSA with SHA-256,
// instead of the original CUP which used HMAC-SHA1 with a random signing key
// encrypted using RSA.
//
// Each ClientUpdateProtocolEcdsa object represents a single update check in
// flight -- a call to SignRequest() generates internal state that will be used
// by ValidateResponse().
class ClientUpdateProtocolEcdsa {
 public:
  ~ClientUpdateProtocolEcdsa();

  // Initializes this instance of CUP-ECDSA with a versioned public key.
  // |key_version| must be non-negative. |public_key| is expected to be a
  // DER-encoded ASN.1 SubjectPublicKeyInfo containing an ECDSA public key.
  // Returns a NULL pointer on failure.
  static scoped_ptr<ClientUpdateProtocolEcdsa> Create(
      int key_version,
      const base::StringPiece& public_key);

  // Generates freshness/authentication data for an outgoing update check.
  // |request_body| contains the body of the update check request in UTF-8.
  // On return, |query_params| contains a set of query parameters (in UTF-8)
  // to be appended to the URL.
  //
  // This method will store internal state in this instance used by calls to
  // ValidateResponse(); if you need to have multiple update checks in flight,
  // initialize a separate CUP-ECDSA instance for each one.
  void SignRequest(const base::StringPiece& request_body,
                   std::string* query_params);

  // Validates a response given to a update check request previously signed
  // with SignRequest(). |response_body| contains the body of the response in
  // UTF-8. |server_proof| contains the ECDSA signature and observed request
  // hash, which is passed in the ETag HTTP header. Returns true if the
  // response is valid and the observed request hash matches the sent hash.
  // This method uses internal state that is set by a prior SignRequest() call.
  bool ValidateResponse(const base::StringPiece& response_body,
                        const base::StringPiece& server_etag);

 private:
  friend class CupEcdsaTest;

  ClientUpdateProtocolEcdsa(int key_version,
                            const base::StringPiece& public_key);

  // The server keeps multiple signing keys; a version must be sent so that
  // the correct signing key is used to sign the assembled message.
  const int pub_key_version_;

  // The ECDSA public key to use for verifying response signatures.
  const std::vector<uint8_t> public_key_;

  // The SHA-256 hash of the XML request.  This is modified on each call to
  // SignRequest(), and checked by ValidateResponse().
  std::vector<uint8_t> request_hash_;

  // The query string containing key version and nonce in UTF-8 form.  This is
  // modified on each call to SignRequest(), and checked by ValidateResponse().
  std::string request_query_cup2key_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ClientUpdateProtocolEcdsa);
};

#endif  // COMPONENTS_UPDATE_CLIENT_CLIENT_UPDATE_PROTOCOL_ECDSA_H_
