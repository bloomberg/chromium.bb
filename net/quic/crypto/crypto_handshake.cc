// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_handshake.h"

#include <ctype.h>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "crypto/secure_hash.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/cert_compressor.h"
#include "net/quic/crypto/channel_id.h"
#include "net/quic/crypto/common_cert_set.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/curve25519_key_exchange.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/p256_key_exchange.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using base::StringPiece;
using base::StringPrintf;
using std::map;
using std::string;
using std::vector;

namespace net {

CryptoHandshakeMessage::CryptoHandshakeMessage()
    : tag_(0),
      minimum_size_(0) {}

CryptoHandshakeMessage::CryptoHandshakeMessage(
    const CryptoHandshakeMessage& other)
    : tag_(other.tag_),
      tag_value_map_(other.tag_value_map_),
      minimum_size_(other.minimum_size_) {
  // Don't copy serialized_. scoped_ptr doesn't have a copy constructor.
  // The new object can lazily reconstruct serialized_.
}

CryptoHandshakeMessage::~CryptoHandshakeMessage() {}

CryptoHandshakeMessage& CryptoHandshakeMessage::operator=(
    const CryptoHandshakeMessage& other) {
  tag_ = other.tag_;
  tag_value_map_ = other.tag_value_map_;
  // Don't copy serialized_. scoped_ptr doesn't have an assignment operator.
  // However, invalidate serialized_.
  serialized_.reset();
  minimum_size_ = other.minimum_size_;
  return *this;
}

void CryptoHandshakeMessage::Clear() {
  tag_ = 0;
  tag_value_map_.clear();
  minimum_size_ = 0;
  serialized_.reset();
}

const QuicData& CryptoHandshakeMessage::GetSerialized() const {
  if (!serialized_.get()) {
    serialized_.reset(CryptoFramer::ConstructHandshakeMessage(*this));
  }
  return *serialized_.get();
}

void CryptoHandshakeMessage::MarkDirty() {
  serialized_.reset();
}

void CryptoHandshakeMessage::Insert(QuicTagValueMap::const_iterator begin,
                                    QuicTagValueMap::const_iterator end) {
  tag_value_map_.insert(begin, end);
}

void CryptoHandshakeMessage::SetTaglist(QuicTag tag, ...) {
  // Warning, if sizeof(QuicTag) > sizeof(int) then this function will break
  // because the terminating 0 will only be promoted to int.
  COMPILE_ASSERT(sizeof(QuicTag) <= sizeof(int),
                 crypto_tag_may_not_be_larger_than_int_or_varargs_will_break);

  vector<QuicTag> tags;
  va_list ap;

  va_start(ap, tag);
  for (;;) {
    QuicTag list_item = va_arg(ap, QuicTag);
    if (list_item == 0) {
      break;
    }
    tags.push_back(list_item);
  }

  // Because of the way that we keep tags in memory, we can copy the contents
  // of the vector and get the correct bytes in wire format. See
  // crypto_protocol.h. This assumes that the system is little-endian.
  SetVector(tag, tags);

  va_end(ap);
}

void CryptoHandshakeMessage::SetStringPiece(QuicTag tag, StringPiece value) {
  tag_value_map_[tag] = value.as_string();
}

void CryptoHandshakeMessage::Erase(QuicTag tag) {
  tag_value_map_.erase(tag);
}

QuicErrorCode CryptoHandshakeMessage::GetTaglist(QuicTag tag,
                                                 const QuicTag** out_tags,
                                                 size_t* out_len) const {
  QuicTagValueMap::const_iterator it = tag_value_map_.find(tag);
  QuicErrorCode ret = QUIC_NO_ERROR;

  if (it == tag_value_map_.end()) {
    ret = QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  } else if (it->second.size() % sizeof(QuicTag) != 0) {
    ret = QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (ret != QUIC_NO_ERROR) {
    *out_tags = NULL;
    *out_len = 0;
    return ret;
  }

  *out_tags = reinterpret_cast<const QuicTag*>(it->second.data());
  *out_len = it->second.size() / sizeof(QuicTag);
  return ret;
}

bool CryptoHandshakeMessage::GetStringPiece(QuicTag tag,
                                            StringPiece* out) const {
  QuicTagValueMap::const_iterator it = tag_value_map_.find(tag);
  if (it == tag_value_map_.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

QuicErrorCode CryptoHandshakeMessage::GetNthValue24(QuicTag tag,
                                                    unsigned index,
                                                    StringPiece* out) const {
  StringPiece value;
  if (!GetStringPiece(tag, &value)) {
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  for (unsigned i = 0;; i++) {
    if (value.empty()) {
      return QUIC_CRYPTO_MESSAGE_INDEX_NOT_FOUND;
    }
    if (value.size() < 3) {
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(value.data());
    size_t size = static_cast<size_t>(data[0]) |
                  (static_cast<size_t>(data[1]) << 8) |
                  (static_cast<size_t>(data[2]) << 16);
    value.remove_prefix(3);

    if (value.size() < size) {
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    if (i == index) {
      *out = StringPiece(value.data(), size);
      return QUIC_NO_ERROR;
    }

    value.remove_prefix(size);
  }
}

QuicErrorCode CryptoHandshakeMessage::GetUint16(QuicTag tag,
                                                uint16* out) const {
  return GetPOD(tag, out, sizeof(uint16));
}

QuicErrorCode CryptoHandshakeMessage::GetUint32(QuicTag tag,
                                                uint32* out) const {
  return GetPOD(tag, out, sizeof(uint32));
}

QuicErrorCode CryptoHandshakeMessage::GetUint64(QuicTag tag,
                                                uint64* out) const {
  return GetPOD(tag, out, sizeof(uint64));
}

size_t CryptoHandshakeMessage::size() const {
  size_t ret = sizeof(QuicTag) +
               sizeof(uint16) /* number of entries */ +
               sizeof(uint16) /* padding */;
  ret += (sizeof(QuicTag) + sizeof(uint32) /* end offset */) *
         tag_value_map_.size();
  for (QuicTagValueMap::const_iterator i = tag_value_map_.begin();
       i != tag_value_map_.end(); ++i) {
    ret += i->second.size();
  }

  return ret;
}

void CryptoHandshakeMessage::set_minimum_size(size_t min_bytes) {
  if (min_bytes == minimum_size_) {
    return;
  }
  serialized_.reset();
  minimum_size_ = min_bytes;
}

size_t CryptoHandshakeMessage::minimum_size() const {
  return minimum_size_;
}

string CryptoHandshakeMessage::DebugString() const {
  return DebugStringInternal(0);
}

QuicErrorCode CryptoHandshakeMessage::GetPOD(
    QuicTag tag, void* out, size_t len) const {
  QuicTagValueMap::const_iterator it = tag_value_map_.find(tag);
  QuicErrorCode ret = QUIC_NO_ERROR;

  if (it == tag_value_map_.end()) {
    ret = QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  } else if (it->second.size() != len) {
    ret = QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (ret != QUIC_NO_ERROR) {
    memset(out, 0, len);
    return ret;
  }

  memcpy(out, it->second.data(), len);
  return ret;
}

string CryptoHandshakeMessage::DebugStringInternal(size_t indent) const {
  string ret = string(2 * indent, ' ') + QuicUtils::TagToString(tag_) + "<\n";
  ++indent;
  for (QuicTagValueMap::const_iterator it = tag_value_map_.begin();
       it != tag_value_map_.end(); ++it) {
    ret += string(2 * indent, ' ') + QuicUtils::TagToString(it->first) + ": ";

    bool done = false;
    switch (it->first) {
      case kKATO:
      case kVERS:
        // uint32 value
        if (it->second.size() == 4) {
          uint32 value;
          memcpy(&value, it->second.data(), sizeof(value));
          ret += base::UintToString(value);
          done = true;
        }
        break;
      case kKEXS:
      case kAEAD:
      case kCGST:
      case kPDMD:
        // tag lists
        if (it->second.size() % sizeof(QuicTag) == 0) {
          for (size_t j = 0; j < it->second.size(); j += sizeof(QuicTag)) {
            QuicTag tag;
            memcpy(&tag, it->second.data() + j, sizeof(tag));
            if (j > 0) {
              ret += ",";
            }
            ret += QuicUtils::TagToString(tag);
          }
          done = true;
        }
        break;
      case kSCFG:
        // nested messages.
        if (!it->second.empty()) {
          scoped_ptr<CryptoHandshakeMessage> msg(
              CryptoFramer::ParseMessage(it->second));
          if (msg.get()) {
            ret += "\n";
            ret += msg->DebugStringInternal(indent + 1);

            done = true;
          }
        }
        break;
      case kPAD:
        ret += StringPrintf("(%d bytes of padding)",
                            static_cast<int>(it->second.size()));
        done = true;
        break;
    }

    if (!done) {
      // If there's no specific format for this tag, or the value is invalid,
      // then just use hex.
      ret += base::HexEncode(it->second.data(), it->second.size());
    }
    ret += "\n";
  }
  --indent;
  ret += string(2 * indent, ' ') + ">";
  return ret;
}

QuicCryptoNegotiatedParameters::QuicCryptoNegotiatedParameters()
    : version(0),
      key_exchange(0),
      aead(0) {
}

QuicCryptoNegotiatedParameters::~QuicCryptoNegotiatedParameters() {}

CrypterPair::CrypterPair() {}

CrypterPair::~CrypterPair() {}

// static
const char QuicCryptoConfig::kInitialLabel[] = "QUIC key expansion";

// static
const char QuicCryptoConfig::kCETVLabel[] = "QUIC CETV block";

// static
const char QuicCryptoConfig::kForwardSecureLabel[] =
    "QUIC forward secure key expansion";

QuicCryptoConfig::QuicCryptoConfig()
    : version(0),
      common_cert_sets(CommonCertSets::GetInstanceQUIC()) {
}

QuicCryptoConfig::~QuicCryptoConfig() {}

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

void QuicCryptoClientConfig::SetDefaults() {
  // Version must be 0.
  // TODO(agl): this version stuff is obsolete now.
  version = QuicCryptoConfig::CONFIG_VERSION;

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
  out->SetValue(kVERS, version);

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

    if (!cached->proof_valid()) {
      // If we are expecting a certificate chain, double the size of the client
      // hello so that the response from the server can be larger - hopefully
      // including the whole certificate chain.
      out->set_minimum_size(kClientHelloMinimumSize * 2);
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
    const CachedState* cached,
    QuicWallTime now,
    QuicRandom* rand,
    QuicCryptoNegotiatedParameters* out_params,
    CryptoHandshakeMessage* out,
    string* error_details) const {
  DCHECK(error_details != NULL);

  FillInchoateClientHello(server_hostname, cached, out_params, out);

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
      return QUIC_INTERNAL_ERROR;
    }

    cetv.SetStringPiece(kCIDK, key);
    cetv.SetStringPiece(kCIDS, signature);

    CrypterPair crypters;
    CryptoUtils::DeriveKeys(out_params->initial_premaster_secret,
                            out_params->aead, out_params->client_nonce,
                            out_params->server_nonce, hkdf_input,
                            CryptoUtils::CLIENT, &crypters);

    const QuicData& cetv_plaintext = cetv.GetSerialized();
    scoped_ptr<QuicData> cetv_ciphertext(crypters.encrypter->EncryptPacket(
        0 /* sequence number */,
        StringPiece() /* associated data */,
        cetv_plaintext.AsStringPiece()));

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

  CryptoUtils::DeriveKeys(out_params->initial_premaster_secret,
                          out_params->aead, out_params->client_nonce,
                          out_params->server_nonce, hkdf_input,
                          CryptoUtils::CLIENT, &out_params->initial_crypters);

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessRejection(
    CachedState* cached,
    const CryptoHandshakeMessage& rej,
    QuicWallTime now,
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
  if (rej.GetStringPiece(kPROF, &proof) &&
      rej.GetStringPiece(kCertificateTag, &cert_bytes)) {
    vector<string> certs;
    if (!CertCompressor::DecompressChain(cert_bytes, out_params->cached_certs,
                                         common_cert_sets, &certs)) {
      *error_details = "Certificate data invalid";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    cached->SetProof(certs, proof);
  }

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    QuicGuid guid,
    QuicCryptoNegotiatedParameters* out_params,
    string* error_details) {
  DCHECK(error_details != NULL);

  if (server_hello.tag() != kSHLO) {
    *error_details = "Bad tag";
    return QUIC_INVALID_CRYPTO_MESSAGE_TYPE;
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

  CryptoUtils::DeriveKeys(
      out_params->forward_secure_premaster_secret, out_params->aead,
      out_params->client_nonce, out_params->server_nonce, hkdf_input,
      CryptoUtils::CLIENT, &out_params->forward_secure_crypters);

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

}  // namespace net
