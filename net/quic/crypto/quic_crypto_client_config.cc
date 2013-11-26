// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_client_config.h"

#include "base/stl_util.h"
#include "net/quic/crypto/cert_compressor.h"
#include "net/quic/crypto/channel_id.h"
#include "net/quic/crypto/common_cert_set.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/curve25519_key_exchange.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/p256_key_exchange.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using base::StringPiece;
using std::map;
using std::string;
using std::vector;

namespace net {

QuicCryptoClientConfig::QuicCryptoClientConfig() {}

QuicCryptoClientConfig::~QuicCryptoClientConfig() {
  STLDeleteValues(&cached_states_);
}

QuicCryptoClientConfig::CachedState::CachedState()
    : server_config_valid_(false),
      generation_counter_(0) {}

QuicCryptoClientConfig::CachedState::~CachedState() {}

bool QuicCryptoClientConfig::CachedState::IsComplete(QuicWallTime now) const {
  if (server_config_.empty() || !server_config_valid_) {
    return false;
  }

  const CryptoHandshakeMessage* scfg = GetServerConfig();
  if (!scfg) {
    // Should be impossible short of cache corruption.
    DCHECK(false);
    return false;
  }

  uint64 expiry_seconds;
  if (scfg->GetUint64(kEXPY, &expiry_seconds) != QUIC_NO_ERROR ||
      now.ToUNIXSeconds() >= expiry_seconds) {
    return false;
  }

  return true;
}

const CryptoHandshakeMessage*
QuicCryptoClientConfig::CachedState::GetServerConfig() const {
  if (server_config_.empty()) {
    return NULL;
  }

  if (!scfg_.get()) {
    scfg_.reset(CryptoFramer::ParseMessage(server_config_));
    DCHECK(scfg_.get());
  }
  return scfg_.get();
}

QuicErrorCode QuicCryptoClientConfig::CachedState::SetServerConfig(
    StringPiece server_config, QuicWallTime now, string* error_details) {
  const bool matches_existing = server_config == server_config_;

  // Even if the new server config matches the existing one, we still wish to
  // reject it if it has expired.
  scoped_ptr<CryptoHandshakeMessage> new_scfg_storage;
  const CryptoHandshakeMessage* new_scfg;

  if (!matches_existing) {
    new_scfg_storage.reset(CryptoFramer::ParseMessage(server_config));
    new_scfg = new_scfg_storage.get();
  } else {
    new_scfg = GetServerConfig();
  }

  if (!new_scfg) {
    *error_details = "SCFG invalid";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  uint64 expiry_seconds;
  if (new_scfg->GetUint64(kEXPY, &expiry_seconds) != QUIC_NO_ERROR) {
    *error_details = "SCFG missing EXPY";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (now.ToUNIXSeconds() >= expiry_seconds) {
    *error_details = "SCFG has expired";
    return QUIC_CRYPTO_SERVER_CONFIG_EXPIRED;
  }

  if (!matches_existing) {
    server_config_ = server_config.as_string();
    SetProofInvalid();
    scfg_.reset(new_scfg_storage.release());
  }
  return QUIC_NO_ERROR;
}

void QuicCryptoClientConfig::CachedState::InvalidateServerConfig() {
  server_config_.clear();
  scfg_.reset();
  SetProofInvalid();
}

void QuicCryptoClientConfig::CachedState::SetProof(const vector<string>& certs,
                                                   StringPiece signature) {
  bool has_changed =
      signature != server_config_sig_ || certs_.size() != certs.size();

  if (!has_changed) {
    for (size_t i = 0; i < certs_.size(); i++) {
      if (certs_[i] != certs[i]) {
        has_changed = true;
        break;
      }
    }
  }

  if (!has_changed) {
    return;
  }

  // If the proof has changed then it needs to be revalidated.
  SetProofInvalid();
  certs_ = certs;
  server_config_sig_ = signature.as_string();
}

void QuicCryptoClientConfig::CachedState::ClearProof() {
  SetProofInvalid();
  certs_.clear();
  server_config_sig_.clear();
}

void QuicCryptoClientConfig::CachedState::SetProofValid() {
  server_config_valid_ = true;
}

void QuicCryptoClientConfig::CachedState::SetProofInvalid() {
  server_config_valid_ = false;
  ++generation_counter_;
}

const string& QuicCryptoClientConfig::CachedState::server_config() const {
  return server_config_;
}

const string&
QuicCryptoClientConfig::CachedState::source_address_token() const {
  return source_address_token_;
}

const vector<string>& QuicCryptoClientConfig::CachedState::certs() const {
  return certs_;
}

const string& QuicCryptoClientConfig::CachedState::signature() const {
  return server_config_sig_;
}

bool QuicCryptoClientConfig::CachedState::proof_valid() const {
  return server_config_valid_;
}

uint64 QuicCryptoClientConfig::CachedState::generation_counter() const {
  return generation_counter_;
}

const ProofVerifyDetails*
QuicCryptoClientConfig::CachedState::proof_verify_details() const {
  return proof_verify_details_.get();
}

void QuicCryptoClientConfig::CachedState::set_source_address_token(
    StringPiece token) {
  source_address_token_ = token.as_string();
}

void QuicCryptoClientConfig::CachedState::SetProofVerifyDetails(
    ProofVerifyDetails* details) {
  proof_verify_details_.reset(details);
}

void QuicCryptoClientConfig::CachedState::InitializeFrom(
    const QuicCryptoClientConfig::CachedState& other) {
  DCHECK(server_config_.empty());
  DCHECK(!server_config_valid_);
  server_config_ = other.server_config_;
  source_address_token_ = other.source_address_token_;
  certs_ = other.certs_;
  server_config_sig_ = other.server_config_sig_;
  server_config_valid_ = other.server_config_valid_;
}

void QuicCryptoClientConfig::SetDefaults() {
  // Key exchange methods.
  kexs.resize(2);
  kexs[0] = kC255;
  kexs[1] = kP256;

  // Authenticated encryption algorithms.
  aead.resize(1);
  aead[0] = kAESG;
}

QuicCryptoClientConfig::CachedState* QuicCryptoClientConfig::LookupOrCreate(
    const string& server_hostname) {
  map<string, CachedState*>::const_iterator it =
      cached_states_.find(server_hostname);
  if (it != cached_states_.end()) {
    return it->second;
  }

  CachedState* cached = new CachedState;
  cached_states_.insert(make_pair(server_hostname, cached));
  return cached;
}

void QuicCryptoClientConfig::FillInchoateClientHello(
    const string& server_hostname,
    const QuicVersion preferred_version,
    const CachedState* cached,
    QuicCryptoNegotiatedParameters* out_params,
    CryptoHandshakeMessage* out) const {
  out->set_tag(kCHLO);
  out->set_minimum_size(kClientHelloMinimumSize);

  // Server name indication. We only send SNI if it's a valid domain name, as
  // per the spec.
  if (CryptoUtils::IsValidSNI(server_hostname)) {
    out->SetStringPiece(kSNI, server_hostname);
  }
  // TODO(rch): Remove once we remove QUIC_VERSION_12.
  out->SetValue(kVERS, static_cast<uint16>(0));
  out->SetValue(kVER, QuicVersionToQuicTag(preferred_version));

  if (!cached->source_address_token().empty()) {
    out->SetStringPiece(kSourceAddressTokenTag, cached->source_address_token());
  }

  if (proof_verifier_.get()) {
    // Don't request ECDSA proofs on platforms that do not support ECDSA
    // certificates.
    bool disableECDSA = false;
#if defined(OS_WIN)
    if (base::win::GetVersion() < base::win::VERSION_VISTA)
      disableECDSA = true;
#endif
    if (disableECDSA) {
      out->SetTaglist(kPDMD, kX59R, 0);
    } else {
      out->SetTaglist(kPDMD, kX509, 0);
    }
  }

  if (common_cert_sets) {
    out->SetStringPiece(kCCS, common_cert_sets->GetCommonHashes());
  }

  const vector<string>& certs = cached->certs();
  // We save |certs| in the QuicCryptoNegotiatedParameters so that, if the
  // client config is being used for multiple connections, another connection
  // doesn't update the cached certificates and cause us to be unable to
  // process the server's compressed certificate chain.
  out_params->cached_certs = certs;
  if (!certs.empty()) {
    vector<uint64> hashes;
    hashes.reserve(certs.size());
    for (vector<string>::const_iterator i = certs.begin();
         i != certs.end(); ++i) {
      hashes.push_back(QuicUtils::FNV1a_64_Hash(i->data(), i->size()));
    }
    out->SetVector(kCCRT, hashes);
  }
}

QuicErrorCode QuicCryptoClientConfig::FillClientHello(
    const string& server_hostname,
    QuicGuid guid,
    const QuicVersion preferred_version,
    const CachedState* cached,
    QuicWallTime now,
    QuicRandom* rand,
    QuicCryptoNegotiatedParameters* out_params,
    CryptoHandshakeMessage* out,
    string* error_details) const {
  DCHECK(error_details != NULL);

  FillInchoateClientHello(server_hostname, preferred_version, cached,
                          out_params, out);

  const CryptoHandshakeMessage* scfg = cached->GetServerConfig();
  if (!scfg) {
    // This should never happen as our caller should have checked
    // cached->IsComplete() before calling this function.
    *error_details = "Handshake not ready";
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  StringPiece scid;
  if (!scfg->GetStringPiece(kSCID, &scid)) {
    *error_details = "SCFG missing SCID";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }
  out->SetStringPiece(kSCID, scid);

  const QuicTag* their_aeads;
  const QuicTag* their_key_exchanges;
  size_t num_their_aeads, num_their_key_exchanges;
  if (scfg->GetTaglist(kAEAD, &their_aeads,
                       &num_their_aeads) != QUIC_NO_ERROR ||
      scfg->GetTaglist(kKEXS, &their_key_exchanges,
                       &num_their_key_exchanges) != QUIC_NO_ERROR) {
    *error_details = "Missing AEAD or KEXS";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  size_t key_exchange_index;
  if (!QuicUtils::FindMutualTag(
          aead, their_aeads, num_their_aeads, QuicUtils::PEER_PRIORITY,
          &out_params->aead, NULL) ||
      !QuicUtils::FindMutualTag(
          kexs, their_key_exchanges, num_their_key_exchanges,
          QuicUtils::PEER_PRIORITY, &out_params->key_exchange,
          &key_exchange_index)) {
    *error_details = "Unsupported AEAD or KEXS";
    return QUIC_CRYPTO_NO_SUPPORT;
  }
  out->SetTaglist(kAEAD, out_params->aead, 0);
  out->SetTaglist(kKEXS, out_params->key_exchange, 0);

  StringPiece public_value;
  if (scfg->GetNthValue24(kPUBS, key_exchange_index, &public_value) !=
          QUIC_NO_ERROR) {
    *error_details = "Missing public value";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  StringPiece orbit;
  if (!scfg->GetStringPiece(kORBT, &orbit) || orbit.size() != kOrbitSize) {
    *error_details = "SCFG missing OBIT";
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  CryptoUtils::GenerateNonce(now, rand, orbit, &out_params->client_nonce);
  out->SetStringPiece(kNONC, out_params->client_nonce);
  if (!out_params->server_nonce.empty()) {
    out->SetStringPiece(kServerNonceTag, out_params->server_nonce);
  }

  switch (out_params->key_exchange) {
    case kC255:
      out_params->client_key_exchange.reset(Curve25519KeyExchange::New(
          Curve25519KeyExchange::NewPrivateKey(rand)));
      break;
    case kP256:
      out_params->client_key_exchange.reset(P256KeyExchange::New(
          P256KeyExchange::NewPrivateKey()));
      break;
    default:
      DCHECK(false);
      *error_details = "Configured to support an unknown key exchange";
      return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  if (!out_params->client_key_exchange->CalculateSharedKey(
          public_value, &out_params->initial_premaster_secret)) {
    *error_details = "Key exchange failure";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }
  out->SetStringPiece(kPUBS, out_params->client_key_exchange->public_value());

  bool do_channel_id = false;
  if (channel_id_signer_.get()) {
    const QuicTag* their_proof_demands;
    size_t num_their_proof_demands;
    if (scfg->GetTaglist(kPDMD, &their_proof_demands,
                         &num_their_proof_demands) == QUIC_NO_ERROR) {
      for (size_t i = 0; i < num_their_proof_demands; i++) {
        if (their_proof_demands[i] == kCHID) {
          do_channel_id = true;
          break;
        }
      }
    }
  }

  if (do_channel_id) {
    // In order to calculate the encryption key for the CETV block we need to
    // serialise the client hello as it currently is (i.e. without the CETV
    // block). For this, the client hello is serialized without padding.
    const size_t orig_min_size = out->minimum_size();
    out->set_minimum_size(0);

    CryptoHandshakeMessage cetv;
    cetv.set_tag(kCETV);

    string hkdf_input;
    const QuicData& client_hello_serialized = out->GetSerialized();
    hkdf_input.append(QuicCryptoConfig::kCETVLabel,
                      strlen(QuicCryptoConfig::kCETVLabel) + 1);
    hkdf_input.append(reinterpret_cast<char*>(&guid), sizeof(guid));
    hkdf_input.append(client_hello_serialized.data(),
                      client_hello_serialized.length());
    hkdf_input.append(cached->server_config());

    string key, signature;
    if (!channel_id_signer_->Sign(server_hostname, hkdf_input,
                                  &key, &signature)) {
      *error_details = "Channel ID signature failed";
      return QUIC_INVALID_CHANNEL_ID_SIGNATURE;
    }

    cetv.SetStringPiece(kCIDK, key);
    cetv.SetStringPiece(kCIDS, signature);

    CrypterPair crypters;
    if (!CryptoUtils::DeriveKeys(out_params->initial_premaster_secret,
                                 out_params->aead, out_params->client_nonce,
                                 out_params->server_nonce, hkdf_input,
                                 CryptoUtils::CLIENT, &crypters)) {
      *error_details = "Symmetric key setup failed";
      return QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED;
    }

    const QuicData& cetv_plaintext = cetv.GetSerialized();
    scoped_ptr<QuicData> cetv_ciphertext(crypters.encrypter->EncryptPacket(
        0 /* sequence number */,
        StringPiece() /* associated data */,
        cetv_plaintext.AsStringPiece()));
    if (!cetv_ciphertext.get()) {
      *error_details = "Packet encryption failed";
      return QUIC_ENCRYPTION_FAILURE;
    }

    out->SetStringPiece(kCETV, cetv_ciphertext->AsStringPiece());
    out->MarkDirty();

    out->set_minimum_size(orig_min_size);
  }

  out_params->hkdf_input_suffix.clear();
  out_params->hkdf_input_suffix.append(reinterpret_cast<char*>(&guid),
                                       sizeof(guid));
  const QuicData& client_hello_serialized = out->GetSerialized();
  out_params->hkdf_input_suffix.append(client_hello_serialized.data(),
                                       client_hello_serialized.length());
  out_params->hkdf_input_suffix.append(cached->server_config());

  string hkdf_input;
  const size_t label_len = strlen(QuicCryptoConfig::kInitialLabel) + 1;
  hkdf_input.reserve(label_len + out_params->hkdf_input_suffix.size());
  hkdf_input.append(QuicCryptoConfig::kInitialLabel, label_len);
  hkdf_input.append(out_params->hkdf_input_suffix);

  if (!CryptoUtils::DeriveKeys(
           out_params->initial_premaster_secret, out_params->aead,
           out_params->client_nonce, out_params->server_nonce, hkdf_input,
           CryptoUtils::CLIENT, &out_params->initial_crypters)) {
    *error_details = "Symmetric key setup failed";
    return QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED;
  }

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessRejection(
    const CryptoHandshakeMessage& rej,
    QuicWallTime now,
    CachedState* cached,
    QuicCryptoNegotiatedParameters* out_params,
    string* error_details) {
  DCHECK(error_details != NULL);

  if (rej.tag() != kREJ) {
    *error_details = "Message is not REJ";
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  StringPiece scfg;
  if (!rej.GetStringPiece(kSCFG, &scfg)) {
    *error_details = "Missing SCFG";
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  QuicErrorCode error = cached->SetServerConfig(scfg, now, error_details);
  if (error != QUIC_NO_ERROR) {
    return error;
  }

  StringPiece token;
  if (rej.GetStringPiece(kSourceAddressTokenTag, &token)) {
    cached->set_source_address_token(token);
  }

  StringPiece nonce;
  if (rej.GetStringPiece(kServerNonceTag, &nonce)) {
    out_params->server_nonce = nonce.as_string();
  }

  StringPiece proof, cert_bytes;
  bool has_proof = rej.GetStringPiece(kPROF, &proof);
  bool has_cert = rej.GetStringPiece(kCertificateTag, &cert_bytes);
  if (has_proof && has_cert) {
    vector<string> certs;
    if (!CertCompressor::DecompressChain(cert_bytes, out_params->cached_certs,
                                         common_cert_sets, &certs)) {
      *error_details = "Certificate data invalid";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    cached->SetProof(certs, proof);
  } else {
    cached->ClearProof();
    if (has_proof && !has_cert) {
      *error_details = "Certificate missing";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    if (!has_proof && has_cert) {
      *error_details = "Proof missing";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }
  }

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    QuicGuid guid,
    const QuicVersionVector& negotiated_versions,
    CachedState* cached,
    QuicCryptoNegotiatedParameters* out_params,
    string* error_details) {
  DCHECK(error_details != NULL);

  if (server_hello.tag() != kSHLO) {
    *error_details = "Bad tag";
    return QUIC_INVALID_CRYPTO_MESSAGE_TYPE;
  }

  const QuicTag* supported_version_tags;
  size_t num_supported_versions;
  // TODO(rch): Once QUIC_VERSION_12 is removed, then make it a failure
  // if the server does not have a version list.
  if (server_hello.GetTaglist(kVER, &supported_version_tags,
                              &num_supported_versions) == QUIC_NO_ERROR) {
    if (!negotiated_versions.empty()) {
      bool mismatch = num_supported_versions != negotiated_versions.size();
      for (size_t i = 0; i < num_supported_versions && !mismatch; ++i) {
        mismatch = QuicTagToQuicVersion(supported_version_tags[i]) !=
            negotiated_versions[i];
      }
      // The server sent a list of supported versions, and the connection
      // reports that there was a version negotiation during the handshake.
      // Ensure that these two lists are identical.
      if (mismatch) {
        *error_details = "Downgrade attack detected";
        return QUIC_VERSION_NEGOTIATION_MISMATCH;
      }
    }
  }

  // Learn about updated source address tokens.
  StringPiece token;
  if (server_hello.GetStringPiece(kSourceAddressTokenTag, &token)) {
    cached->set_source_address_token(token);
  }

  // TODO(agl):
  //   learn about updated SCFGs.

  StringPiece public_value;
  if (!server_hello.GetStringPiece(kPUBS, &public_value)) {
    *error_details = "server hello missing forward secure public value";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (!out_params->client_key_exchange->CalculateSharedKey(
          public_value, &out_params->forward_secure_premaster_secret)) {
    *error_details = "Key exchange failure";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  string hkdf_input;
  const size_t label_len = strlen(QuicCryptoConfig::kForwardSecureLabel) + 1;
  hkdf_input.reserve(label_len + out_params->hkdf_input_suffix.size());
  hkdf_input.append(QuicCryptoConfig::kForwardSecureLabel, label_len);
  hkdf_input.append(out_params->hkdf_input_suffix);

  if (!CryptoUtils::DeriveKeys(
           out_params->forward_secure_premaster_secret, out_params->aead,
           out_params->client_nonce, out_params->server_nonce, hkdf_input,
           CryptoUtils::CLIENT, &out_params->forward_secure_crypters)) {
    *error_details = "Symmetric key setup failed";
    return QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED;
  }

  return QUIC_NO_ERROR;
}

ProofVerifier* QuicCryptoClientConfig::proof_verifier() const {
  return proof_verifier_.get();
}

void QuicCryptoClientConfig::SetProofVerifier(ProofVerifier* verifier) {
  proof_verifier_.reset(verifier);
}

ChannelIDSigner* QuicCryptoClientConfig::channel_id_signer() const {
  return channel_id_signer_.get();
}

void QuicCryptoClientConfig::SetChannelIDSigner(ChannelIDSigner* signer) {
  channel_id_signer_.reset(signer);
}

void QuicCryptoClientConfig::InitializeFrom(
    const std::string& server_hostname,
    const std::string& canonical_server_hostname,
    QuicCryptoClientConfig* canonical_crypto_config) {
  CachedState* canonical_cached =
      canonical_crypto_config->LookupOrCreate(canonical_server_hostname);
  if (!canonical_cached->proof_valid()) {
    return;
  }
  CachedState* cached = LookupOrCreate(server_hostname);
  cached->InitializeFrom(*canonical_cached);
}

}  // namespace net
