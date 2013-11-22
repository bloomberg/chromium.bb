// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_verifier_chromium.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "crypto/signature_verifier.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/single_request_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/ssl/ssl_config_service.h"

using base::StringPiece;
using base::StringPrintf;
using std::string;
using std::vector;

namespace net {

ProofVerifierChromium::ProofVerifierChromium(CertVerifier* cert_verifier,
                                             const BoundNetLog& net_log)
  : cert_verifier_(cert_verifier),
    next_state_(STATE_NONE),
    net_log_(net_log) {
}

ProofVerifierChromium::~ProofVerifierChromium() {
  verifier_.reset();
}

ProofVerifierChromium::Status ProofVerifierChromium::VerifyProof(
    const string& hostname,
    const string& server_config,
    const vector<string>& certs,
    const string& signature,
    std::string* error_details,
    scoped_ptr<ProofVerifyDetails>* details,
    ProofVerifierCallback* callback) {
  DCHECK(error_details);
  DCHECK(details);
  DCHECK(callback);

  callback_.reset(callback);
  error_details->clear();

  DCHECK_EQ(STATE_NONE, next_state_);
  if (STATE_NONE != next_state_) {
    *error_details = "Certificate is already set and VerifyProof has begun";
    DLOG(WARNING) << *error_details;
    return FAILURE;
  }

  verify_details_.reset(new ProofVerifyDetailsChromium);

  if (certs.empty()) {
    *error_details = "Failed to create certificate chain. Certs are empty.";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    details->reset(verify_details_.release());
    return FAILURE;
  }

  // Convert certs to X509Certificate.
  vector<StringPiece> cert_pieces(certs.size());
  for (unsigned i = 0; i < certs.size(); i++) {
    cert_pieces[i] = base::StringPiece(certs[i]);
  }
  cert_ = X509Certificate::CreateFromDERCertChain(cert_pieces);
  if (!cert_.get()) {
    *error_details = "Failed to create certificate chain";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    details->reset(verify_details_.release());
    return FAILURE;
  }

  // We call VerifySignature first to avoid copying of server_config and
  // signature.
  if (!VerifySignature(server_config, signature, certs[0])) {
    *error_details = "Failed to verify signature of server config";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    details->reset(verify_details_.release());
    return FAILURE;
  }

  hostname_ = hostname;

  next_state_ = STATE_VERIFY_CERT;
  switch (DoLoop(OK)) {
    case OK:
      details->reset(verify_details_.release());
      return SUCCESS;
    case ERR_IO_PENDING:
      return PENDING;
    default:
      *error_details = error_details_;
      details->reset(verify_details_.release());
      return FAILURE;
  }
}

int ProofVerifierChromium::DoLoop(int last_result) {
  int rv = last_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

void ProofVerifierChromium::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    scoped_ptr<ProofVerifyDetails> scoped_details(verify_details_.release());
    callback_->Run(rv == OK, error_details_, &scoped_details);
    callback_.reset();
  }
}

int ProofVerifierChromium::DoVerifyCert(int result) {
  next_state_ = STATE_VERIFY_CERT_COMPLETE;

  int flags = 0;
  verifier_.reset(new SingleRequestCertVerifier(cert_verifier_));
  return verifier_->Verify(
      cert_.get(),
      hostname_,
      flags,
      SSLConfigService::GetCRLSet().get(),
      &verify_details_->cert_verify_result,
      base::Bind(&ProofVerifierChromium::OnIOComplete,
                 base::Unretained(this)),
      net_log_);
}

int ProofVerifierChromium::DoVerifyCertComplete(int result) {
  verifier_.reset();

  if (result <= ERR_FAILED) {
    error_details_ = StringPrintf("Failed to verify certificate chain: %s",
                                  ErrorToString(result));
    DLOG(WARNING) << error_details_;
    result = ERR_FAILED;
  }

  // Exit DoLoop and return the result to the caller to VerifyProof.
  DCHECK_EQ(STATE_NONE, next_state_);
  return result;
}

bool ProofVerifierChromium::VerifySignature(const string& signed_data,
                                            const string& signature,
                                            const string& cert) {
  StringPiece spki;
  if (!asn1::ExtractSPKIFromDERCert(cert, &spki)) {
    DLOG(WARNING) << "ExtractSPKIFromDERCert failed";
    return false;
  }

  crypto::SignatureVerifier verifier;

  size_t size_bits;
  X509Certificate::PublicKeyType type;
  X509Certificate::GetPublicKeyInfo(cert_->os_cert_handle(), &size_bits,
                                    &type);
  if (type == X509Certificate::kPublicKeyTypeRSA) {
    crypto::SignatureVerifier::HashAlgorithm hash_alg =
        crypto::SignatureVerifier::SHA256;
    crypto::SignatureVerifier::HashAlgorithm mask_hash_alg = hash_alg;
    unsigned int hash_len = 32;  // 32 is the length of a SHA-256 hash.

    bool ok = verifier.VerifyInitRSAPSS(
        hash_alg, mask_hash_alg, hash_len,
        reinterpret_cast<const uint8*>(signature.data()), signature.size(),
        reinterpret_cast<const uint8*>(spki.data()), spki.size());
    if (!ok) {
      DLOG(WARNING) << "VerifyInitRSAPSS failed";
      return false;
    }
  } else if (type == X509Certificate::kPublicKeyTypeECDSA) {
    // This is the algorithm ID for ECDSA with SHA-256. Parameters are ABSENT.
    // RFC 5758:
    //   ecdsa-with-SHA256 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
    //        us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 2 }
    //   ...
    //   When the ecdsa-with-SHA224, ecdsa-with-SHA256, ecdsa-with-SHA384, or
    //   ecdsa-with-SHA512 algorithm identifier appears in the algorithm field
    //   as an AlgorithmIdentifier, the encoding MUST omit the parameters
    //   field.  That is, the AlgorithmIdentifier SHALL be a SEQUENCE of one
    //   component, the OID ecdsa-with-SHA224, ecdsa-with-SHA256, ecdsa-with-
    //   SHA384, or ecdsa-with-SHA512.
    // See also RFC 5480, Appendix A.
    static const uint8 kECDSAWithSHA256AlgorithmID[] = {
      0x30, 0x0a,
        0x06, 0x08,
          0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02,
    };

    if (!verifier.VerifyInit(
            kECDSAWithSHA256AlgorithmID, sizeof(kECDSAWithSHA256AlgorithmID),
            reinterpret_cast<const uint8*>(signature.data()),
            signature.size(),
            reinterpret_cast<const uint8*>(spki.data()),
            spki.size())) {
      DLOG(WARNING) << "VerifyInit failed";
      return false;
    }
  } else {
    LOG(ERROR) << "Unsupported public key type " << type;
    return false;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(kProofSignatureLabel),
                        sizeof(kProofSignatureLabel));
  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(signed_data.data()),
                        signed_data.size());

  if (!verifier.VerifyFinal()) {
    DLOG(WARNING) << "VerifyFinal failed";
    return false;
  }

  DVLOG(1) << "VerifyFinal success";
  return true;
}

}  // namespace net
