// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_handshake.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "crypto/hkdf.h"
#include "crypto/secure_hash.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/curve25519_key_exchange.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_protocol.h"

using base::StringPiece;
using crypto::SecureHash;
using std::string;
using std::vector;

namespace net {
// kVersion contains the one (and, for the moment, only) version number that we
// implement.
static const uint16 kVersion = 0;

static const char kLabel[] = "QUIC key expansion";

using crypto::SecureHash;

QuicServerConfigProtobuf::QuicServerConfigProtobuf() {
}

QuicServerConfigProtobuf::~QuicServerConfigProtobuf() {
  STLDeleteElements(&keys_);
}

CryptoHandshakeMessage::CryptoHandshakeMessage() {}

CryptoHandshakeMessage::CryptoHandshakeMessage(
    const CryptoHandshakeMessage& other)
    : tag(other.tag),
      tag_value_map(other.tag_value_map) {
  // Don't copy serialized_. scoped_ptr doesn't have a copy constructor.
  // The new object can reconstruct serialized_ lazily.
}

CryptoHandshakeMessage::~CryptoHandshakeMessage() {}

CryptoHandshakeMessage& CryptoHandshakeMessage::operator=(
    const CryptoHandshakeMessage& other) {
  tag = other.tag;
  tag_value_map = other.tag_value_map;
  // Don't copy serialized_. scoped_ptr doesn't have an assignment operator.
  // However, invalidate serialized_.
  serialized_.reset();
  return *this;
}

const QuicData& CryptoHandshakeMessage::GetSerialized() const {
  if (!serialized_.get()) {
    serialized_.reset(CryptoFramer::ConstructHandshakeMessage(*this));
  }
  return *serialized_.get();
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
    CryptoTag tag = va_arg(ap, CryptoTag);
    if (tag == 0) {
      break;
    }
    tags.push_back(tag);
  }

  // Because of the way that we keep tags in memory, we can copy the contents
  // of the vector and get the correct bytes in wire format. See
  // crypto_protocol.h. This assumes that the system is little-endian.
  SetVector(tag, tags);

  va_end(ap);
}

QuicErrorCode CryptoHandshakeMessage::GetTaglist(CryptoTag tag,
                                                 const CryptoTag** out_tags,
                                                 size_t* out_len) const {
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  QuicErrorCode ret = QUIC_NO_ERROR;

  if (it == tag_value_map.end()) {
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
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  if (it == tag_value_map.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

QuicErrorCode CryptoHandshakeMessage::GetNthValue16(
    CryptoTag tag,
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
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  if (it == tag_value_map.end()) {
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

QuicErrorCode CryptoHandshakeMessage::GetPOD(
    CryptoTag tag, void* out, size_t len) const {
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  QuicErrorCode ret = QUIC_NO_ERROR;

  if (it == tag_value_map.end()) {
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

QuicCryptoNegotiatedParams::QuicCryptoNegotiatedParams()
    : version(0),
      key_exchange(0),
      aead(0) {
}

QuicCryptoNegotiatedParams::~QuicCryptoNegotiatedParams() {
}

QuicCryptoConfig::QuicCryptoConfig()
    : version(0) {
}

QuicCryptoConfig::~QuicCryptoConfig() {
  STLDeleteElements(&key_exchanges);
}

bool QuicCryptoConfig::ProcessPeerHandshake(
    const CryptoHandshakeMessage& peer_msg,
    CryptoUtils::Priority priority,
    QuicCryptoNegotiatedParams* out_params,
    string* error_details) const {
  if (peer_msg.GetUint16(kVERS, &out_params->version) != QUIC_NO_ERROR ||
      out_params->version != kVersion) {
    if (error_details) {
      *error_details = "Bad version";
    }
    return false;
  }

  const CryptoTag* their_aeads;
  const CryptoTag* their_key_exchanges;
  size_t num_their_aeads, num_their_key_exchanges;
  if (peer_msg.GetTaglist(kAEAD, &their_aeads,
                          &num_their_aeads) != QUIC_NO_ERROR ||
      peer_msg.GetTaglist(kKEXS, &their_key_exchanges,
                          &num_their_key_exchanges) != QUIC_NO_ERROR) {
    if (error_details) {
      *error_details = "Missing AEAD or KEXS";
    }
    return false;
  }

  size_t key_exchange_index;
  if (!CryptoUtils::FindMutualTag(aead,
                                  their_aeads, num_their_aeads,
                                  priority,
                                  &out_params->aead,
                                  NULL) ||
      !CryptoUtils::FindMutualTag(kexs,
                                  their_key_exchanges, num_their_key_exchanges,
                                  priority,
                                  &out_params->key_exchange,
                                  &key_exchange_index)) {
    if (error_details) {
      *error_details = "Unsupported AEAD or KEXS";
    }
    return false;
  }

  StringPiece public_value;
  if (peer_msg.GetNthValue16(kPUBS, key_exchange_index, &public_value) !=
      QUIC_NO_ERROR) {
    if (error_details) {
      *error_details = "Missing public value";
    }
    return false;
  }

  KeyExchange* key_exchange = NULL;
  for (vector<KeyExchange*>::const_iterator i = key_exchanges.begin();
       i != key_exchanges.end(); ++i) {
    if ((*i)->tag() == out_params->key_exchange) {
      key_exchange = *i;
      break;
    }
  }
  DCHECK(key_exchange != NULL);

  if (!key_exchange->CalculateSharedKey(public_value,
                                        &out_params->premaster_secret)) {
    if (error_details) {
      *error_details = "Key exchange failure";
    }
    return false;
  }

  return true;
}

QuicCryptoClientConfig::QuicCryptoClientConfig()
    : hkdf_info(kLabel, arraysize(kLabel)) {
}

void QuicCryptoClientConfig::SetDefaults(QuicRandom* rand) {
  // Version must be 0.
  version = kVersion;

  // Key exchange methods.
  const string private_key = Curve25519KeyExchange::NewPrivateKey(rand);
  key_exchanges.clear();
  key_exchanges.push_back(Curve25519KeyExchange::New(private_key));
  kexs.resize(1);
  kexs[0] = kC255;

  // Authenticated encryption algorithms.
  aead.resize(1);
  aead[0] = kAESG;
}

void QuicCryptoClientConfig::FillClientHello(const string& nonce,
                                             const string& server_hostname,
                                             CryptoHandshakeMessage* out) {
  out->tag = kCHLO;

  out->SetValue(kVERS, version);
  out->SetVector(kKEXS, kexs);
  out->SetVector(kAEAD, aead);
  out->tag_value_map[kNONC] = nonce;

  // Build the public values tag.
  size_t pubs_len = 0;
  for (vector<KeyExchange*>::const_iterator i = key_exchanges.begin();
       i != key_exchanges.end(); ++i) {
    pubs_len += 2 /* length bytes */;
    pubs_len += (*i)->public_value().size();
  }

  size_t pubs_offset = 0;
  scoped_ptr<uint8[]> pubs(new uint8[pubs_len]);
  for (vector<KeyExchange*>::const_iterator i = key_exchanges.begin();
       i != key_exchanges.end(); ++i) {
    StringPiece pub = (*i)->public_value();
    pubs[pubs_offset++] = static_cast<uint8>(pub.size());
    pubs[pubs_offset++] = static_cast<uint8>(pub.size() >> 8);
    memcpy(&pubs[pubs_offset], pub.data(), pub.size());
    pubs_offset += pub.size();
  }
  out->tag_value_map[kPUBS] =
      string(reinterpret_cast<char*>(pubs.get()), pubs_len);

  // Server name indication.
  // If server_hostname is not an IP address literal, it is a DNS hostname.
  IPAddressNumber ip;
  if (!server_hostname.empty() &&
      !ParseIPLiteralToNumber(server_hostname, &ip)) {
    out->tag_value_map[kSNI] = server_hostname;
  }
}

QuicErrorCode QuicCryptoClientConfig::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    const string& nonce,
    QuicCryptoNegotiatedParams* out_params,
    string* error_details) {
  if (server_hello.tag != kSHLO) {
    *error_details = "Bad tag";
    return QUIC_INVALID_CRYPTO_MESSAGE_TYPE;
  }

  StringPiece scfg_bytes;
  if (!server_hello.GetStringPiece(kSCFG, &scfg_bytes)) {
    *error_details = "Missing SCFG";
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  scoped_ptr<CryptoHandshakeMessage> scfg(
      CryptoFramer::ParseMessage(scfg_bytes));
  if (!scfg.get() || scfg->tag != kSCFG) {
    *error_details = "Invalid SCFG";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (!ProcessPeerHandshake(*scfg, CryptoUtils::PEER_PRIORITY, out_params,
                            error_details)) {
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  hkdf_info.append(scfg_bytes.data(), scfg_bytes.size());

  out_params->encrypter.reset(QuicEncrypter::Create(out_params->aead));
  out_params->decrypter.reset(QuicDecrypter::Create(out_params->aead));
  size_t key_bytes = out_params->encrypter->GetKeySize();
  size_t nonce_prefix_bytes = out_params->encrypter->GetNoncePrefixSize();
  uint32 key_length_in_bits = 8 * 2 * (key_bytes + nonce_prefix_bytes);
  hkdf_info.append(reinterpret_cast<char*>(&key_length_in_bits),
                   sizeof(key_length_in_bits));

  crypto::HKDF hkdf(out_params->premaster_secret, nonce,
                    hkdf_info, key_bytes, nonce_prefix_bytes);
  out_params->encrypter->SetKey(hkdf.client_write_key());
  out_params->encrypter->SetNoncePrefix(hkdf.client_write_iv());
  out_params->decrypter->SetKey(hkdf.server_write_key());
  out_params->decrypter->SetNoncePrefix(hkdf.server_write_iv());

  return QUIC_NO_ERROR;
}


QuicCryptoServerConfig::QuicCryptoServerConfig()
    : hkdf_info(kLabel, arraysize(kLabel)) {
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
  const string public_value = curve25519->public_value().as_string();

  char len_bytes[2];
  len_bytes[0] = public_value.size();
  len_bytes[1] = public_value.size() >> 8;

  msg.tag = kSCFG;
  msg.SetTaglist(kKEXS, kC255, 0);
  msg.SetTaglist(kAEAD, kAESG, 0);
  msg.SetValue(kVERS, static_cast<uint16>(0));
  msg.tag_value_map[kPUBS] =
      string(len_bytes, sizeof(len_bytes)) + public_value;
  msg.tag_value_map.insert(extra_tags.tag_value_map.begin(),
                           extra_tags.tag_value_map.end());

  scoped_ptr<QuicData> serialized(
      CryptoFramer::ConstructHandshakeMessage(msg));

  scoped_ptr<QuicServerConfigProtobuf> config(new QuicServerConfigProtobuf);
  config->set_config(serialized->AsStringPiece());
  QuicServerConfigProtobuf::PrivateKey* key = config->add_key();
  key->set_tag(kC255);
  key->set_private_key(private_key);

  return config.release();
}

CryptoTagValueMap* QuicCryptoServerConfig::AddConfig(
    QuicServerConfigProtobuf* protobuf) {
  scoped_ptr<CryptoHandshakeMessage> msg(
      CryptoFramer::ParseMessage(protobuf->config()));

  if (!msg.get()) {
    LOG(WARNING) << "Failed to parse server config message";
    return NULL;
  }
  if (msg->tag != kSCFG) {
    LOG(WARNING) << "Server config message has tag "
                 << msg->tag << " expected "
                 << kSCFG;
    return NULL;
  }

  scoped_ptr<Config> config(new Config);
  config->serialized = protobuf->config();

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

  return new CryptoTagValueMap(msg->tag_value_map);
}

CryptoTagValueMap* QuicCryptoServerConfig::AddTestingConfig(
    QuicRandom* rand,
    const QuicClock* clock,
    const CryptoHandshakeMessage& extra_tags) {
  scoped_ptr<QuicServerConfigProtobuf> config(ConfigForTesting(
      rand, clock, extra_tags));
  return AddConfig(config.get());
}

bool QuicCryptoServerConfig::ProcessClientHello(
    const CryptoHandshakeMessage& client_hello,
    const string& nonce,
    CryptoHandshakeMessage* out,
    QuicCryptoNegotiatedParams *out_params,
    string* error_details) {
  CHECK(!configs_.empty());
  // FIXME(agl): we should use the client's SCID, not just the active config.
  const Config* config(configs_[active_config_]);

  out->tag = kREJ;
  out->tag_value_map.clear();

  if (!config->ProcessPeerHandshake(client_hello,
                                    CryptoUtils::LOCAL_PRIORITY,
                                    out_params,
                                    error_details)) {
    return false;
  }

  StringPiece client_nonce;
  if (!client_hello.GetStringPiece(kNONC, &client_nonce)) {
    return false;
  }

  const QuicData& client_hello_serialized = client_hello.GetSerialized();
  hkdf_info.append(client_hello_serialized.data(),
                   client_hello_serialized.length());
  hkdf_info.append(config->serialized);

  out_params->encrypter.reset(QuicEncrypter::Create(out_params->aead));
  out_params->decrypter.reset(QuicDecrypter::Create(out_params->aead));
  size_t key_bytes = out_params->encrypter->GetKeySize();
  size_t nonce_prefix_bytes = out_params->encrypter->GetNoncePrefixSize();
  uint32 key_length_in_bits = 8 * 2 * (key_bytes + nonce_prefix_bytes);
  hkdf_info.append(reinterpret_cast<char*>(&key_length_in_bits),
                   sizeof(key_length_in_bits));

  crypto::HKDF hkdf(out_params->premaster_secret, client_nonce,
                    hkdf_info, key_bytes, nonce_prefix_bytes);
  out_params->encrypter->SetKey(hkdf.server_write_key());
  out_params->encrypter->SetNoncePrefix(hkdf.server_write_iv());
  out_params->decrypter->SetKey(hkdf.client_write_key());
  out_params->decrypter->SetNoncePrefix(hkdf.client_write_iv());

  // TODO(agl): This is obviously missing most of the handshake.
  out->tag = kSHLO;
  out->tag_value_map[kNONC] = nonce;
  out->tag_value_map[kSCFG] = config->serialized;
  return true;
}

QuicCryptoServerConfig::Config::Config() {
}

QuicCryptoServerConfig::Config::~Config() {
}

}  // namespace net
