// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_handshake.h"

#include <ctype.h>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/hkdf.h"
#include "crypto/secure_hash.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/curve25519_key_exchange.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/p256_key_exchange.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_protocol.h"

using base::StringPiece;
using crypto::SecureHash;
using std::map;
using std::string;
using std::vector;

namespace net {

// kVersion contains the one (and, for the moment, only) version number that we
// implement.
static const uint16 kVersion = 0;

// kLabel is constant that is used in key derivation to tie the resulting key
// to this protocol.
static const char kLabel[] = "QUIC key expansion";

using crypto::SecureHash;

QuicServerConfigProtobuf::QuicServerConfigProtobuf() {
}

QuicServerConfigProtobuf::~QuicServerConfigProtobuf() {
  STLDeleteElements(&keys_);
}

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

void CryptoHandshakeMessage::SetStringPiece(CryptoTag tag,
                                            StringPiece value) {
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


QuicCryptoConfig::QuicCryptoConfig()
    : version(0) {
}

QuicCryptoConfig::~QuicCryptoConfig() {
}


QuicCryptoClientConfig::QuicCryptoClientConfig() {
}

QuicCryptoClientConfig::~QuicCryptoClientConfig() {
  STLDeleteValues(&cached_states_);
}

QuicCryptoClientConfig::CachedState::CachedState() {
}

QuicCryptoClientConfig::CachedState::~CachedState() {
}

bool QuicCryptoClientConfig::CachedState::is_complete() const {
  return !server_config_.empty();
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

const string& QuicCryptoClientConfig::CachedState::server_config()
    const {
  return server_config_;
}

const string& QuicCryptoClientConfig::CachedState::source_address_token()
    const {
  return source_address_token_;
}

const string& QuicCryptoClientConfig::CachedState::orbit() const {
  return orbit_;
}

void QuicCryptoClientConfig::SetDefaults() {
  // Version must be 0.
  version = kVersion;

  // Key exchange methods.
  kexs.resize(1);
  kexs[0] = kC255;

  // Authenticated encryption algorithms.
  aead.resize(1);
  aead[0] = kAESG;
}

const QuicCryptoClientConfig::CachedState* QuicCryptoClientConfig::Lookup(
    const string& server_hostname) {
  map<string, CachedState*>::const_iterator it =
      cached_states_.find(server_hostname);
  if (it == cached_states_.end()) {
    return NULL;
  }
  return it->second;
}

void QuicCryptoClientConfig::FillInchoateClientHello(
    const string& server_hostname,
    const CachedState* cached,
    CryptoHandshakeMessage* out) {
  out->set_tag(kCHLO);

  // Server name indication.
  // If server_hostname is not an IP address literal, it is a DNS hostname.
  IPAddressNumber ip;
  if (!server_hostname.empty() &&
      !ParseIPLiteralToNumber(server_hostname, &ip)) {
    out->SetStringPiece(kSNI, server_hostname);
  }
  out->SetValue(kVERS, version);

  if (cached && !cached->source_address_token().empty()) {
    out->SetValue(kSRCT, cached->source_address_token());
  }
}

QuicErrorCode QuicCryptoClientConfig::FillClientHello(
    const string& server_hostname,
    QuicGuid guid,
    const CachedState* cached,
    const QuicClock* clock,
    QuicRandom* rand,
    QuicCryptoNegotiatedParameters* out_params,
    CryptoHandshakeMessage* out,
    string* error_details) {
  FillInchoateClientHello(server_hostname, cached, out);

  const CryptoHandshakeMessage* scfg = cached->GetServerConfig();
  if (!scfg) {
    // This should never happen as our caller should have checked
    // cached->is_complete() before calling this function.
    if (error_details) {
      *error_details = "Handshake not ready";
    }
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  StringPiece scid;
  if (!scfg->GetStringPiece(kSCID, &scid)) {
    if (error_details) {
      *error_details = "SCFG missing SCID";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }
  out->SetStringPiece(kSCID, scid);

  // Calculate the mutual algorithms that the connection is going to use.
  if (scfg->GetUint16(kVERS, &out_params->version) != QUIC_NO_ERROR ||
      out_params->version != kVersion) {
    if (error_details) {
      *error_details = "Bad version";
    }
    return QUIC_VERSION_NOT_SUPPORTED;
  }

  const CryptoTag* their_aeads;
  const CryptoTag* their_key_exchanges;
  size_t num_their_aeads, num_their_key_exchanges;
  if (scfg->GetTaglist(kAEAD, &their_aeads,
                       &num_their_aeads) != QUIC_NO_ERROR ||
      scfg->GetTaglist(kKEXS, &their_key_exchanges,
                       &num_their_key_exchanges) != QUIC_NO_ERROR) {
    if (error_details) {
      *error_details = "Missing AEAD or KEXS";
    }
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
    if (error_details) {
      *error_details = "Unsupported AEAD or KEXS";
    }
    return QUIC_CRYPTO_NO_SUPPORT;
  }
  out->SetTaglist(kAEAD, out_params->aead, 0);
  out->SetTaglist(kKEXS, out_params->key_exchange, 0);

  StringPiece public_value;
  if (scfg->GetNthValue16(kPUBS, key_exchange_index, &public_value) !=
      QUIC_NO_ERROR) {
    if (error_details) {
      *error_details = "Missing public value";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  string nonce;
  CryptoUtils::GenerateNonce(clock, rand, cached->orbit(), &nonce);
  out->SetStringPiece(kNONC, nonce);

  scoped_ptr<KeyExchange> key_exchange;
  switch (out_params->key_exchange) {
  case kC255:
    key_exchange.reset(Curve25519KeyExchange::New(
          Curve25519KeyExchange::NewPrivateKey(rand)));
    break;
  case kP256:
    key_exchange.reset(P256KeyExchange::New(
          Curve25519KeyExchange::NewPrivateKey(rand)));
    break;
  default:
    DCHECK(false);
    if (error_details) {
      *error_details = "Configured to support an unknown key exchange";
    }
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  if (!key_exchange->CalculateSharedKey(public_value,
                                        &out_params->premaster_secret)) {
    if (error_details) {
      *error_details = "Key exchange failure";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }
  out->SetStringPiece(kPUBS, key_exchange->public_value());

  string hkdf_input(kLabel, arraysize(kLabel));
  hkdf_input.append(reinterpret_cast<char*>(&guid), sizeof(guid));

  const QuicData& client_hello_serialized = out->GetSerialized();
  hkdf_input.append(client_hello_serialized.data(),
                   client_hello_serialized.length());
  hkdf_input.append(cached->server_config());

  CryptoUtils::DeriveKeys(out_params, nonce, hkdf_input, CryptoUtils::CLIENT);

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessRejection(
    const string& server_hostname,
    const CryptoHandshakeMessage& rej,
    string* error_details) {
  CachedState* cached;

  map<string, CachedState*>::const_iterator it =
      cached_states_.find(server_hostname);
  if (it == cached_states_.end()) {
    cached = new CachedState;
    cached_states_[server_hostname] = cached;
  } else {
    cached = it->second;
  }

  StringPiece scfg;
  if (!rej.GetStringPiece(kSCFG, &scfg)) {
    if (error_details) {
      *error_details = "Missing SCFG";
    }
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  if (!cached->SetServerConfig(scfg)) {
    if (error_details) {
      *error_details = "Invalid SCFG";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicCryptoClientConfig::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    const string& nonce,
    QuicCryptoNegotiatedParameters* out_params,
    string* error_details) {
  if (server_hello.tag() != kSHLO) {
    *error_details = "Bad tag";
    return QUIC_INVALID_CRYPTO_MESSAGE_TYPE;
  }

  // TODO(agl):
  //   learn about updated SCFGs.
  //   read ephemeral public value for forward-secret keys.

  return QUIC_NO_ERROR;
}


QuicCryptoServerConfig::QuicCryptoServerConfig() {
}

QuicCryptoServerConfig::~QuicCryptoServerConfig() {
  STLDeleteValues(&configs_);
}

// static
QuicServerConfigProtobuf* QuicCryptoServerConfig::ConfigForTesting(
    QuicRandom* rand,
    const QuicClock* clock,
    const CryptoHandshakeMessage& extra_tags)  {
  CryptoHandshakeMessage msg;

  const string private_key = Curve25519KeyExchange::NewPrivateKey(rand);
  scoped_ptr<Curve25519KeyExchange> curve25519(
      Curve25519KeyExchange::New(private_key));
  StringPiece public_value = curve25519->public_value();
  string encoded_public_value;
  // First two bytes encode the length of the public value.
  encoded_public_value.push_back(public_value.size());
  encoded_public_value.push_back(public_value.size() >> 8);
  encoded_public_value.append(public_value.data(), public_value.size());

  msg.set_tag(kSCFG);
  msg.SetTaglist(kKEXS, kC255, 0);
  msg.SetTaglist(kAEAD, kAESG, 0);
  msg.SetValue(kVERS, static_cast<uint16>(0));
  msg.SetStringPiece(kPUBS, encoded_public_value);
  msg.Insert(extra_tags.tag_value_map().begin(),
             extra_tags.tag_value_map().end());

  char scid_bytes[16];
  rand->RandBytes(scid_bytes, sizeof(scid_bytes));
  msg.SetStringPiece(kSCID, StringPiece(scid_bytes, sizeof(scid_bytes)));

  scoped_ptr<QuicData> serialized(
      CryptoFramer::ConstructHandshakeMessage(msg));

  scoped_ptr<QuicServerConfigProtobuf> config(new QuicServerConfigProtobuf);
  config->set_config(serialized->AsStringPiece());
  QuicServerConfigProtobuf::PrivateKey* key = config->add_key();
  key->set_tag(kC255);
  key->set_private_key(private_key);

  return config.release();
}

CryptoHandshakeMessage* QuicCryptoServerConfig::AddConfig(
    QuicServerConfigProtobuf* protobuf) {
  scoped_ptr<CryptoHandshakeMessage> msg(
      CryptoFramer::ParseMessage(protobuf->config()));

  if (!msg.get()) {
    LOG(WARNING) << "Failed to parse server config message";
    return NULL;
  }
  if (msg->tag() != kSCFG) {
    LOG(WARNING) << "Server config message has tag "
                 << msg->tag() << " expected "
                 << kSCFG;
    return NULL;
  }

  scoped_ptr<Config> config(new Config);
  config->serialized = protobuf->config();

  StringPiece scid;
  if (!msg->GetStringPiece(kSCID, &scid)) {
    LOG(WARNING) << "Server config message is missing SCID";
    return NULL;
  }
  config->id = scid.as_string();

  const CryptoTag* aead_tags;
  size_t aead_len;
  if (msg->GetTaglist(kAEAD, &aead_tags, &aead_len) != QUIC_NO_ERROR) {
    LOG(WARNING) << "Server config message is missing AEAD";
    return NULL;
  }
  config->aead = vector<CryptoTag>(aead_tags, aead_tags + aead_len);

  const CryptoTag* kexs_tags;
  size_t kexs_len;
  if (msg->GetTaglist(kKEXS, &kexs_tags, &kexs_len) != QUIC_NO_ERROR) {
    LOG(WARNING) << "Server config message is missing KEXS";
    return NULL;
  }

  if (kexs_len != protobuf->key_size()) {
    LOG(WARNING) << "Server config has "
                 << kexs_len
                 << " key exchange methods configured, but "
                 << protobuf->key_size()
                 << " private keys";
    return NULL;
  }

  for (size_t i = 0; i < kexs_len; i++) {
    const CryptoTag tag = kexs_tags[i];
    string private_key;

    config->kexs.push_back(tag);

    for (size_t j = 0; j < protobuf->key_size(); j++) {
      const QuicServerConfigProtobuf::PrivateKey& key = protobuf->key(i);
      if (key.tag() == tag) {
        private_key = key.private_key();
        break;
      }
    }

    if (private_key.empty()) {
      LOG(WARNING) << "Server config contains key exchange method without "
                      "corresponding private key: "
                   << tag;
      return NULL;
    }

    scoped_ptr<KeyExchange> ka;
    switch (tag) {
    case kC255:
      ka.reset(Curve25519KeyExchange::New(private_key));
      if (!ka.get()) {
        LOG(WARNING) << "Server config contained an invalid curve25519"
                        " private key.";
        return NULL;
      }
      break;
    default:
      LOG(WARNING) << "Server config message contains unknown key exchange "
                      "method: "
                   << tag;
      return NULL;
    }

    for (vector<KeyExchange*>::const_iterator i = config->key_exchanges.begin();
         i != config->key_exchanges.end(); ++i) {
      if ((*i)->tag() == tag) {
        LOG(WARNING) << "Duplicate key exchange in config: " << tag;
        return NULL;
      }
    }

    config->key_exchanges.push_back(ka.release());
  }

  if (msg->GetUint16(kVERS, &config->version) != QUIC_NO_ERROR) {
    LOG(WARNING) << "Server config message is missing version";
    return NULL;
  }

  if (config->version != kVersion) {
    LOG(WARNING) << "Server config specifies an unsupported version";
    return NULL;
  }

  scoped_ptr<SecureHash> sha256(SecureHash::Create(SecureHash::SHA256));
  sha256->Update(protobuf->config().data(), protobuf->config().size());
  char id_bytes[16];
  sha256->Finish(id_bytes, sizeof(id_bytes));
  const string id(id_bytes, sizeof(id_bytes));

  configs_[id] = config.release();
  active_config_ = id;

  return msg.release();
}

CryptoHandshakeMessage* QuicCryptoServerConfig::AddTestingConfig(
    QuicRandom* rand,
    const QuicClock* clock,
    const CryptoHandshakeMessage& extra_tags) {
  scoped_ptr<QuicServerConfigProtobuf> config(ConfigForTesting(
      rand, clock, extra_tags));
  return AddConfig(config.get());
}

QuicErrorCode QuicCryptoServerConfig::ProcessClientHello(
    const CryptoHandshakeMessage& client_hello,
    QuicGuid guid,
    CryptoHandshakeMessage* out,
    QuicCryptoNegotiatedParameters *out_params,
    string* error_details) {
  CHECK(!configs_.empty());
  // FIXME(agl): we should use the client's SCID, not just the active config.
  const Config* config(configs_[active_config_]);

  StringPiece scid;
  if (!client_hello.GetStringPiece(kSCID, &scid) ||
      scid.as_string() != config->id) {
    // If the client didn't provide a server config ID, or gave the wrong one,
    // then the handshake cannot possibly complete. We reject the handshake and
    // give the client enough information to do better next time.
    out->Clear();
    out->set_tag(kREJ);
    out->SetStringPiece(kSCFG, config->serialized);
    return QUIC_NO_ERROR;
  }

  const CryptoTag* their_aeads;
  const CryptoTag* their_key_exchanges;
  size_t num_their_aeads, num_their_key_exchanges;
  if (client_hello.GetTaglist(kAEAD, &their_aeads,
                              &num_their_aeads) != QUIC_NO_ERROR ||
      client_hello.GetTaglist(kKEXS, &their_key_exchanges,
                              &num_their_key_exchanges) != QUIC_NO_ERROR ||
      num_their_aeads != 1 ||
      num_their_key_exchanges != 1) {
    if (error_details) {
      *error_details = "Missing or invalid AEAD or KEXS";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  size_t key_exchange_index;
  if (!CryptoUtils::FindMutualTag(config->aead,
                                  their_aeads, num_their_aeads,
                                  CryptoUtils::LOCAL_PRIORITY,
                                  &out_params->aead,
                                  NULL) ||
      !CryptoUtils::FindMutualTag(config->kexs,
                                  their_key_exchanges, num_their_key_exchanges,
                                  CryptoUtils::LOCAL_PRIORITY,
                                  &out_params->key_exchange,
                                  &key_exchange_index)) {
    if (error_details) {
      *error_details = "Unsupported AEAD or KEXS";
    }
    return QUIC_CRYPTO_NO_SUPPORT;
  }

  StringPiece public_value;
  if (!client_hello.GetStringPiece(kPUBS, &public_value)) {
    if (error_details) {
      *error_details = "Missing public value";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (!config->key_exchanges[key_exchange_index]->CalculateSharedKey(
           public_value, &out_params->premaster_secret)) {
    if (error_details) {
      *error_details = "Invalid public value";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  out_params->server_config_id = scid.as_string();

  StringPiece client_nonce;
  if (!client_hello.GetStringPiece(kNONC, &client_nonce)) {
    if (error_details) {
      *error_details = "Invalid nonce";
    }
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  string hkdf_input(kLabel, arraysize(kLabel));
  hkdf_input.append(reinterpret_cast<char*>(&guid), sizeof(guid));

  const QuicData& client_hello_serialized = client_hello.GetSerialized();
  hkdf_input.append(client_hello_serialized.data(),
                    client_hello_serialized.length());
  hkdf_input.append(config->serialized);

  CryptoUtils::DeriveKeys(out_params, client_nonce, hkdf_input,
                          CryptoUtils::SERVER);

  out->set_tag(kSHLO);
  return QUIC_NO_ERROR;
}

QuicCryptoServerConfig::Config::Config() {
}

QuicCryptoServerConfig::Config::~Config() {
  STLDeleteElements(&key_exchanges);
}

}  // namespace net
