// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_handshake.h"

#include <ctype.h>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "crypto/secure_hash.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/curve25519_key_exchange.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/p256_key_exchange.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

using base::StringPiece;
using std::map;
using std::string;
using std::vector;

namespace net {

CryptoHandshakeMessage::CryptoHandshakeMessage() : tag_(0) {}

CryptoHandshakeMessage::CryptoHandshakeMessage(
    const CryptoHandshakeMessage& other)
    : tag_(other.tag_),
      tag_value_map_(other.tag_value_map_) {
  // Don't copy serialized_. scoped_ptr doesn't have a copy constructor.
  // The new object can reconstruct serialized_ lazily.
}

CryptoHandshakeMessage::~CryptoHandshakeMessage() {}

CryptoHandshakeMessage& CryptoHandshakeMessage::operator=(
    const CryptoHandshakeMessage& other) {
  tag_ = other.tag_;
  tag_value_map_ = other.tag_value_map_;
  // Don't copy serialized_. scoped_ptr doesn't have an assignment operator.
  // However, invalidate serialized_.
  serialized_.reset();
  return *this;
}

void CryptoHandshakeMessage::Clear() {
  tag_ = 0;
  tag_value_map_.clear();
  serialized_.reset();
}

const QuicData& CryptoHandshakeMessage::GetSerialized() const {
  if (!serialized_.get()) {
    serialized_.reset(CryptoFramer::ConstructHandshakeMessage(*this));
  }
  return *serialized_.get();
}

void CryptoHandshakeMessage::Insert(CryptoTagValueMap::const_iterator begin,
                                    CryptoTagValueMap::const_iterator end) {
  tag_value_map_.insert(begin, end);
}

void CryptoHandshakeMessage::SetTaglist(CryptoTag tag, ...) {
  // Warning, if sizeof(CryptoTag) > sizeof(int) then this function will break
  // because the terminating 0 will only be promoted to int.
  COMPILE_ASSERT(sizeof(CryptoTag) <= sizeof(int),
                 crypto_tag_not_be_larger_than_int_or_varargs_will_break);

  vector<CryptoTag> tags;
  va_list ap;

  va_start(ap, tag);
  for (;;) {
    CryptoTag list_item = va_arg(ap, CryptoTag);
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

void CryptoHandshakeMessage::SetStringPiece(CryptoTag tag, StringPiece value) {
  tag_value_map_[tag] = value.as_string();
}

QuicErrorCode CryptoHandshakeMessage::GetTaglist(CryptoTag tag,
                                                 const CryptoTag** out_tags,
                                                 size_t* out_len) const {
  CryptoTagValueMap::const_iterator it = tag_value_map_.find(tag);
  QuicErrorCode ret = QUIC_NO_ERROR;

  if (it == tag_value_map_.end()) {
    ret = QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  } else if (it->second.size() % sizeof(CryptoTag) != 0) {
    ret = QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (ret != QUIC_NO_ERROR) {
    *out_tags = NULL;
    *out_len = 0;
    return ret;
  }

  *out_tags = reinterpret_cast<const CryptoTag*>(it->second.data());
  *out_len = it->second.size() / sizeof(CryptoTag);
  return ret;
}

bool CryptoHandshakeMessage::GetStringPiece(CryptoTag tag,
                                            StringPiece* out) const {
  CryptoTagValueMap::const_iterator it = tag_value_map_.find(tag);
  if (it == tag_value_map_.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

QuicErrorCode CryptoHandshakeMessage::GetNthValue16(CryptoTag tag,
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
    if (value.size() < 2) {
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(value.data());
    size_t size = static_cast<size_t>(data[0]) |
                  (static_cast<size_t>(data[1]) << 8);
    value.remove_prefix(2);

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

bool CryptoHandshakeMessage::GetString(CryptoTag tag, string* out) const {
  CryptoTagValueMap::const_iterator it = tag_value_map_.find(tag);
  if (it == tag_value_map_.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

QuicErrorCode CryptoHandshakeMessage::GetUint16(CryptoTag tag,
                                                uint16* out) const {
  return GetPOD(tag, out, sizeof(uint16));
}

QuicErrorCode CryptoHandshakeMessage::GetUint32(CryptoTag tag,
                                                uint32* out) const {
  return GetPOD(tag, out, sizeof(uint32));
}

string CryptoHandshakeMessage::DebugString() const {
  return DebugStringInternal(0);
}

QuicErrorCode CryptoHandshakeMessage::GetPOD(
    CryptoTag tag, void* out, size_t len) const {
  CryptoTagValueMap::const_iterator it = tag_value_map_.find(tag);
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

// TagToString is a utility function for pretty-printing handshake messages
// that converts a tag to a string. It will try to maintain the human friendly
// name if possible (i.e. kABCD -> "ABCD"), or will just treat it as a number
// if not.
static string TagToString(CryptoTag tag) {
  char chars[4];
  bool ascii = true;
  const CryptoTag orig_tag = tag;

  for (size_t i = 0; i < sizeof(chars); i++) {
    chars[i] = tag;
    if (chars[i] == 0 && i == 3) {
      chars[i] = ' ';
    }
    if (!isprint(static_cast<unsigned char>(chars[i]))) {
      ascii = false;
      break;
    }
    tag >>= 8;
  }

  if (ascii) {
    return string(chars, sizeof(chars));
  }

  return base::UintToString(orig_tag);
}

string CryptoHandshakeMessage::DebugStringInternal(size_t indent) const {
  string ret = string(2 * indent, ' ') + TagToString(tag_) + "<\n";
  ++indent;
  for (CryptoTagValueMap::const_iterator it = tag_value_map_.begin();
       it != tag_value_map_.end(); ++it) {
    ret += string(2 * indent, ' ') + TagToString(it->first) + ": ";

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
      if (it->second.size() % sizeof(CryptoTag) == 0) {
        for (size_t j = 0; j < it->second.size(); j += sizeof(CryptoTag)) {
          CryptoTag tag;
          memcpy(&tag, it->second.data() + j, sizeof(tag));
          if (j > 0) {
            ret += ",";
          }
          ret += TagToString(tag);
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

QuicCryptoNegotiatedParameters::~QuicCryptoNegotiatedParameters() {
}


// static
const char QuicCryptoConfig::kLabel[] = "QUIC key expansion";

QuicCryptoConfig::QuicCryptoConfig()
    : version(0) {
}

QuicCryptoConfig::~QuicCryptoConfig() {}

QuicCryptoClientConfig::QuicCryptoClientConfig() {}

QuicCryptoClientConfig::~QuicCryptoClientConfig() {
  STLDeleteValues(&cached_states_);
}

QuicCryptoClientConfig::CachedState::CachedState()
    : server_config_valid_(false) {}

QuicCryptoClientConfig::CachedState::~CachedState() {}

bool QuicCryptoClientConfig::CachedState::is_complete() const {
  return !server_config_.empty() && server_config_valid_;
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

bool QuicCryptoClientConfig::CachedState::SetServerConfig(
    StringPiece scfg) {
  scfg_.reset(CryptoFramer::ParseMessage(scfg));
  if (!scfg_.get()) {
    return false;
  }
  server_config_ = scfg.as_string();
  return true;
}

void QuicCryptoClientConfig::CachedState::SetProof(
    const vector<StringPiece>& certs, StringPiece signature) {
  bool has_changed = signature != server_config_sig_;

  if (certs.size() != certs_.size()) {
    has_changed = true;
  }
  if (!has_changed) {
    for (size_t i = 0; i < certs_.size(); i++) {
      if (certs[i] != certs_[i]) {
        has_changed = true;
        break;
      }
    }
  }

  if (!has_changed) {
    return;
  }

  // If the proof has changed then it needs to be revalidated.
  server_config_valid_ = false;
  certs_.clear();
  for (vector<StringPiece>::const_iterator i = certs.begin();
       i != certs.end(); ++i) {
    certs_.push_back(i->as_string());
  }
  server_config_sig_ = signature.as_string();
}

void QuicCryptoClientConfig::CachedState::SetProofValid() {
  server_config_valid_ = true;
}

const string&
QuicCryptoClientConfig::CachedState::server_config() const {
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

void QuicCryptoClientConfig::CachedState::set_source_address_token(
    StringPiece token) {
  source_address_token_ = token.as_string();
}

void QuicCryptoClientConfig::SetDefaults() {
  // Version must be 0.
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
    CryptoHandshakeMessage* out) const {
  out->set_tag(kCHLO);

  // Server name indication.
  // If server_hostname is not an IP address literal, it is a DNS hostname.
  IPAddressNumber ip;
  if (!server_hostname.empty() &&
      !ParseIPLiteralToNumber(server_hostname, &ip)) {
    out->SetStringPiece(kSNI, server_hostname);
  }
  out->SetValue(kVERS, version);

  if (!cached->source_address_token().empty()) {
    out->SetStringPiece(kSRCT, cached->source_address_token());
  }

  out->SetTaglist(kPDMD, kX509, 0);
}

QuicErrorCode QuicCryptoClientConfig::FillClientHello(
    const string& server_hostname,
    QuicGuid guid,
    const CachedState* cached,
    const QuicClock* clock,
    QuicRandom* rand,
    QuicCryptoNegotiatedParameters* out_params,
    CryptoHandshakeMessage* out,
    string* error_details) const {
  DCHECK(error_details != NULL);

  FillInchoateClientHello(server_hostname, cached, out);

  const CryptoHandshakeMessage* scfg = cached->GetServerConfig();
  if (!scfg) {
    // This should never happen as our caller should have checked
    // cached->is_complete() before calling this function.
    *error_details = "Handshake not ready";
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  StringPiece scid;
  if (!scfg->GetStringPiece(kSCID, &scid)) {
    *error_details = "SCFG missing SCID";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }
  out->SetStringPiece(kSCID, scid);

  // Calculate the mutual algorithms that the connection is going to use.
  if (scfg->GetUint16(kVERS, &out_params->version) != QUIC_NO_ERROR ||
      out_params->version != QuicCryptoConfig::CONFIG_VERSION) {
    *error_details = "Bad version";
    return QUIC_CRYPTO_VERSION_NOT_SUPPORTED;
  }

  const CryptoTag* their_aeads;
  const CryptoTag* their_key_exchanges;
  size_t num_their_aeads, num_their_key_exchanges;
  if (scfg->GetTaglist(kAEAD, &their_aeads,
                       &num_their_aeads) != QUIC_NO_ERROR ||
      scfg->GetTaglist(kKEXS, &their_key_exchanges,
                       &num_their_key_exchanges) != QUIC_NO_ERROR) {
    *error_details = "Missing AEAD or KEXS";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  size_t key_exchange_index;
  if (!CryptoUtils::FindMutualTag(aead,
                                  their_aeads, num_their_aeads,
                                  CryptoUtils::PEER_PRIORITY,
                                  &out_params->aead,
                                  NULL) ||
      !CryptoUtils::FindMutualTag(kexs,
                                  their_key_exchanges, num_their_key_exchanges,
                                  CryptoUtils::PEER_PRIORITY,
                                  &out_params->key_exchange,
                                  &key_exchange_index)) {
    *error_details = "Unsupported AEAD or KEXS";
    return QUIC_CRYPTO_NO_SUPPORT;
  }
  out->SetTaglist(kAEAD, out_params->aead, 0);
  out->SetTaglist(kKEXS, out_params->key_exchange, 0);

  StringPiece public_value;
  if (scfg->GetNthValue16(kPUBS, key_exchange_index, &public_value) !=
          QUIC_NO_ERROR) {
    *error_details = "Missing public value";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  StringPiece orbit;
  if (!scfg->GetStringPiece(kORBT, &orbit) || orbit.size() != kOrbitSize) {
    *error_details = "SCFG missing OBIT";
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  string nonce;
  CryptoUtils::GenerateNonce(clock->NowAsDeltaSinceUnixEpoch(), rand, orbit,
                             &nonce);
  out->SetStringPiece(kNONC, nonce);

  scoped_ptr<KeyExchange> key_exchange;
  switch (out_params->key_exchange) {
  case kC255:
    key_exchange.reset(Curve25519KeyExchange::New(
          Curve25519KeyExchange::NewPrivateKey(rand)));
    break;
  case kP256:
    key_exchange.reset(P256KeyExchange::New(
          P256KeyExchange::NewPrivateKey()));
    break;
  default:
    DCHECK(false);
    *error_details = "Configured to support an unknown key exchange";
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  if (!key_exchange->CalculateSharedKey(public_value,
                                        &out_params->premaster_secret)) {
    *error_details = "Key exchange failure";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }
  out->SetStringPiece(kPUBS, key_exchange->public_value());

  string hkdf_input(QuicCryptoConfig::kLabel,
                    strlen(QuicCryptoConfig::kLabel) + 1);
  hkdf_input.append(reinterpret_cast<char*>(&guid), sizeof(guid));

  const QuicData& client_hello_serialized = out->GetSerialized();
  hkdf_input.append(client_hello_serialized.data(),
                   client_hello_serialized.length());
  hkdf_input.append(cached->server_config());

  CryptoUtils::DeriveKeys(out_params, nonce, hkdf_input, CryptoUtils::CLIENT);

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessRejection(
    CachedState* cached,
    const CryptoHandshakeMessage& rej,
    QuicCryptoNegotiatedParameters* out_params,
    string* error_details) {
  DCHECK(error_details != NULL);

  StringPiece scfg;
  if (!rej.GetStringPiece(kSCFG, &scfg)) {
    *error_details = "Missing SCFG";
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  if (!cached->SetServerConfig(scfg)) {
    *error_details = "Invalid SCFG";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  StringPiece token;
  if (rej.GetStringPiece(kSRCT, &token)) {
    cached->set_source_address_token(token);
  }

  StringPiece nonce;
  if (rej.GetStringPiece(kNONC, &nonce) &&
      nonce.size() == kNonceSize) {
    out_params->server_nonce = nonce.as_string();
  }

  StringPiece proof, cert_bytes;
  if (rej.GetStringPiece(kPROF, &proof) &&
      rej.GetStringPiece(kCERT, &cert_bytes)) {
    vector<StringPiece> certs;
    while (!cert_bytes.empty()) {
      if (cert_bytes.size() < 3) {
        *error_details = "Certificate length truncated";
        return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
      }
      size_t len = static_cast<size_t>(cert_bytes[0]) |
                   static_cast<size_t>(cert_bytes[1]) << 8 |
                   static_cast<size_t>(cert_bytes[2]) << 16;
      if (len == 0) {
        *error_details = "Zero length certificate";
        return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
      }
      cert_bytes.remove_prefix(3);
      if (cert_bytes.size() < len) {
        *error_details = "Certificate truncated";
        return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
      }
      certs.push_back(StringPiece(cert_bytes.data(), len));
      cert_bytes.remove_prefix(len);
    }

    cached->SetProof(certs, proof);
  }

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    const string& nonce,
    QuicCryptoNegotiatedParameters* out_params,
    string* error_details) {
  DCHECK(error_details != NULL);

  if (server_hello.tag() != kSHLO) {
    *error_details = "Bad tag";
    return QUIC_INVALID_CRYPTO_MESSAGE_TYPE;
  }

  // TODO(agl):
  //   learn about updated SCFGs.
  //   read ephemeral public value for forward-secret keys.

  return QUIC_NO_ERROR;
}

const ProofVerifier* QuicCryptoClientConfig::proof_verifier() const {
  return proof_verifier_.get();
}

void QuicCryptoClientConfig::SetProofVerifier(ProofVerifier* verifier) {
  proof_verifier_.reset(verifier);
}

}  // namespace net
