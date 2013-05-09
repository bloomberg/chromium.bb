// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_server_config.h"

#include "base/stl_util.h"
#include "crypto/hkdf.h"
#include "crypto/secure_hash.h"
#include "net/quic/crypto/aes_128_gcm_decrypter.h"
#include "net/quic/crypto/aes_128_gcm_encrypter.h"
#include "net/quic/crypto/cert_compressor.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_server_config_protobuf.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/curve25519_key_exchange.h"
#include "net/quic/crypto/ephemeral_key_source.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/p256_key_exchange.h"
#include "net/quic/crypto/proof_source.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/crypto/source_address_token.h"
#include "net/quic/crypto/strike_register.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using crypto::SecureHash;
using std::map;
using std::string;
using std::vector;

namespace net {

// static
const char QuicCryptoServerConfig::TESTING[] = "secret string for testing";

QuicCryptoServerConfig::QuicCryptoServerConfig(
    StringPiece source_address_token_secret)
    // AES-GCM is used to encrypt and authenticate source address tokens. The
    // full, 96-bit nonce is used but we must ensure that an attacker cannot
    // obtain two source address tokens with the same nonce. This occurs with
    // probability 0.5 after 2**48 values. We assume that obtaining 2**48
    // source address tokens is not possible: at a rate of 10M packets per
    // second, it would still take the attacker a year to obtain the needed
    // number of packets.
    //
    // TODO(agl): switch to an encrypter with a larger nonce space (i.e.
    // Salsa20+Poly1305).
    : strike_register_lock_(),
      source_address_token_encrypter_(new Aes128GcmEncrypter),
      source_address_token_decrypter_(new Aes128GcmDecrypter) {
  crypto::HKDF hkdf(source_address_token_secret, StringPiece() /* no salt */,
                    "QUIC source address token key",
                    source_address_token_encrypter_->GetKeySize(),
                    0 /* no fixed IV needed */);
  source_address_token_encrypter_->SetKey(hkdf.server_write_key());
  source_address_token_decrypter_->SetKey(hkdf.server_write_key());
}

QuicCryptoServerConfig::~QuicCryptoServerConfig() {
  STLDeleteValues(&configs_);
}

// static
QuicServerConfigProtobuf* QuicCryptoServerConfig::DefaultConfig(
    QuicRandom* rand,
    const QuicClock* clock,
    const CryptoHandshakeMessage& extra_tags,
    uint64 expiry_time)  {
  CryptoHandshakeMessage msg;

  const string curve25519_private_key =
      Curve25519KeyExchange::NewPrivateKey(rand);
  scoped_ptr<Curve25519KeyExchange> curve25519(
      Curve25519KeyExchange::New(curve25519_private_key));
  StringPiece curve25519_public_value = curve25519->public_value();

  const string p256_private_key = P256KeyExchange::NewPrivateKey();
  scoped_ptr<P256KeyExchange> p256(P256KeyExchange::New(p256_private_key));
  StringPiece p256_public_value = p256->public_value();

  string encoded_public_values;
  // First three bytes encode the length of the public value.
  encoded_public_values.push_back(curve25519_public_value.size());
  encoded_public_values.push_back(curve25519_public_value.size() >> 8);
  encoded_public_values.push_back(curve25519_public_value.size() >> 16);
  encoded_public_values.append(curve25519_public_value.data(),
                               curve25519_public_value.size());
  encoded_public_values.push_back(p256_public_value.size());
  encoded_public_values.push_back(p256_public_value.size() >> 8);
  encoded_public_values.push_back(p256_public_value.size() >> 16);
  encoded_public_values.append(p256_public_value.data(),
                               p256_public_value.size());

  msg.set_tag(kSCFG);
  msg.SetTaglist(kKEXS, kC255, kP256, 0);
  msg.SetTaglist(kAEAD, kAESG, 0);
  msg.SetValue(kVERS, static_cast<uint16>(0));
  msg.SetStringPiece(kPUBS, encoded_public_values);
  msg.Insert(extra_tags.tag_value_map().begin(),
             extra_tags.tag_value_map().end());

  if (expiry_time == 0) {
    const QuicWallTime now = clock->WallNow();
    const QuicWallTime expiry = now.Add(QuicTime::Delta::FromSeconds(
        60 * 60 * 24 * 180 /* 180 days, ~six months */));
    const uint64 expiry_seconds = expiry.ToUNIXSeconds();
    msg.SetValue(kEXPY, expiry_seconds);
  } else {
    msg.SetValue(kEXPY, expiry_time);
  }

  char scid_bytes[16];
  rand->RandBytes(scid_bytes, sizeof(scid_bytes));
  msg.SetStringPiece(kSCID, StringPiece(scid_bytes, sizeof(scid_bytes)));

  char orbit_bytes[kOrbitSize];
  rand->RandBytes(orbit_bytes, sizeof(orbit_bytes));
  msg.SetStringPiece(kORBT, StringPiece(orbit_bytes, sizeof(orbit_bytes)));

  scoped_ptr<QuicData> serialized(CryptoFramer::ConstructHandshakeMessage(msg));

  scoped_ptr<QuicServerConfigProtobuf> config(new QuicServerConfigProtobuf);
  config->set_config(serialized->AsStringPiece());
  QuicServerConfigProtobuf::PrivateKey* curve25519_key = config->add_key();
  curve25519_key->set_tag(kC255);
  curve25519_key->set_private_key(curve25519_private_key);
  QuicServerConfigProtobuf::PrivateKey* p256_key = config->add_key();
  p256_key->set_tag(kP256);
  p256_key->set_private_key(p256_private_key);

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
    LOG(WARNING) << "Server config message has tag " << msg->tag()
                 << " expected " << kSCFG;
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

  const QuicTag* aead_tags;
  size_t aead_len;
  if (msg->GetTaglist(kAEAD, &aead_tags, &aead_len) != QUIC_NO_ERROR) {
    LOG(WARNING) << "Server config message is missing AEAD";
    return NULL;
  }
  config->aead = vector<QuicTag>(aead_tags, aead_tags + aead_len);

  const QuicTag* kexs_tags;
  size_t kexs_len;
  if (msg->GetTaglist(kKEXS, &kexs_tags, &kexs_len) != QUIC_NO_ERROR) {
    LOG(WARNING) << "Server config message is missing KEXS";
    return NULL;
  }

  StringPiece orbit;
  if (!msg->GetStringPiece(kORBT, &orbit)) {
    LOG(WARNING) << "Server config message is missing OBIT";
    return NULL;
  }

  if (orbit.size() != kOrbitSize) {
    LOG(WARNING) << "Orbit value in server config is the wrong length."
                    " Got " << orbit.size() << " want " << kOrbitSize;
    return NULL;
  }
  COMPILE_ASSERT(sizeof(config->orbit) == kOrbitSize, orbit_incorrect_size);
  memcpy(config->orbit, orbit.data(), sizeof(config->orbit));

  if (kexs_len != protobuf->key_size()) {
    LOG(WARNING) << "Server config has " << kexs_len
                 << " key exchange methods configured, but "
                 << protobuf->key_size() << " private keys";
    return NULL;
  }

  for (size_t i = 0; i < kexs_len; i++) {
    const QuicTag tag = kexs_tags[i];
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
                      "corresponding private key: " << tag;
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
      case kP256:
        ka.reset(P256KeyExchange::New(private_key));
        if (!ka.get()) {
          LOG(WARNING) << "Server config contained an invalid P-256"
                          " private key.";
          return NULL;
        }
        break;
      default:
        LOG(WARNING) << "Server config message contains unknown key exchange "
                        "method: " << tag;
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

  if (config->version != QuicCryptoConfig::CONFIG_VERSION) {
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

CryptoHandshakeMessage* QuicCryptoServerConfig::AddDefaultConfig(
    QuicRandom* rand,
    const QuicClock* clock,
    const CryptoHandshakeMessage& extra_tags,
    uint64 expiry_time) {
  scoped_ptr<QuicServerConfigProtobuf> config(DefaultConfig(
      rand, clock, extra_tags, expiry_time));
  return AddConfig(config.get());
}

QuicErrorCode QuicCryptoServerConfig::ProcessClientHello(
    const CryptoHandshakeMessage& client_hello,
    QuicGuid guid,
    const IPEndPoint& client_ip,
    const QuicClock* clock,
    QuicRandom* rand,
    QuicCryptoNegotiatedParameters *params,
    CryptoHandshakeMessage* out,
    string* error_details) const {
  DCHECK(error_details);

  CHECK(!configs_.empty());

  // FIXME(agl): we should use the client's SCID, not just the active config.
  map<ServerConfigID, Config*>::const_iterator it =
      configs_.find(active_config_);
  if (it == configs_.end()) {
    *error_details = "No valid server config loaded";
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }
  const Config* const config(it->second);

  const QuicWallTime now = clock->WallNow();
  bool valid_source_address_token = false;
  StringPiece srct;
  if (client_hello.GetStringPiece(kSourceAddressTokenTag, &srct) &&
      ValidateSourceAddressToken(srct, client_ip, now)) {
    valid_source_address_token = true;
  }

  const string fresh_source_address_token =
      NewSourceAddressToken(client_ip, rand, now);

  // If we previously sent a REJ to this client then we may have stored a
  // server nonce in |params|. In which case, we know that the connection
  // is unique because the server nonce will be mixed into the key generation.
  bool unique_by_server_nonce = !params->server_nonce.empty();
  // If we can't ensure uniqueness by a server nonce, then we will try and use
  // the strike register.
  bool unique_by_strike_register = false;

  StringPiece client_nonce;
  bool client_nonce_well_formed = false;
  if (client_hello.GetStringPiece(kNONC, &client_nonce) &&
      client_nonce.size() == kNonceSize) {
    client_nonce_well_formed = true;
    base::AutoLock auto_lock(strike_register_lock_);

    if (strike_register_.get() == NULL) {
      strike_register_.reset(new StrikeRegister(
          // TODO(agl): these magic numbers should come from config.
          1024 /* max entries */,
          static_cast<uint32>(now.ToUNIXSeconds()),
          600 /* window secs */, config->orbit));
    }
    unique_by_strike_register = strike_register_->Insert(
        reinterpret_cast<const uint8*>(client_nonce.data()),
        static_cast<uint32>(now.ToUNIXSeconds()));
  }

  StringPiece server_nonce;
  client_hello.GetStringPiece(kServerNonceTag, &server_nonce);
  const bool server_nonce_matches = server_nonce == params->server_nonce;

  out->Clear();

  StringPiece sni;
  client_hello.GetStringPiece(kSNI, &sni);

  StringPiece scid;
  if (!client_hello.GetStringPiece(kSCID, &scid) ||
      scid.as_string() != config->id ||
      !valid_source_address_token ||
      !client_nonce_well_formed ||
      !server_nonce_matches ||
      (!unique_by_strike_register &&
       !unique_by_server_nonce)) {
    // If the client didn't provide a server config ID, or gave the wrong one,
    // then the handshake cannot possibly complete. We reject the handshake and
    // give the client enough information to do better next time.
    out->set_tag(kREJ);
    out->SetStringPiece(kSCFG, config->serialized);
    out->SetStringPiece(kSourceAddressTokenTag, fresh_source_address_token);
    if (params->server_nonce.empty()) {
      CryptoUtils::GenerateNonce(
          now, rand, StringPiece(reinterpret_cast<const char*>(config->orbit),
                                 sizeof(config->orbit)),
          &params->server_nonce);
    }
    out->SetStringPiece(kServerNonceTag, params->server_nonce);

    // The client may have requested a certificate chain.
    const QuicTag* their_proof_demands;
    size_t num_their_proof_demands;

    if (proof_source_.get() != NULL && !sni.empty() &&
        client_hello.GetTaglist(kPDMD, &their_proof_demands,
                                &num_their_proof_demands) == QUIC_NO_ERROR) {
      for (size_t i = 0; i < num_their_proof_demands; i++) {
        if (their_proof_demands[i] != kX509) {
          continue;
        }

        const vector<string>* certs;
        string signature;
        if (!proof_source_->GetProof(sni.as_string(), config->serialized,
                                     &certs, &signature)) {
          break;
        }

        StringPiece their_common_set_hashes;
        StringPiece their_cached_cert_hashes;
        client_hello.GetStringPiece(kCCS, &their_common_set_hashes);
        client_hello.GetStringPiece(kCCRT, &their_cached_cert_hashes);

        const string compressed = CertCompressor::CompressChain(
            *certs, their_common_set_hashes, their_cached_cert_hashes,
            config->common_cert_set_.get());

        // kMaxUnverifiedSize is the number of bytes that the certificate chain
        // and signature can consume before we will demand a valid
        // source-address token.
        static const size_t kMaxUnverifiedSize = 400;
        if (valid_source_address_token ||
            signature.size() + compressed.size() < kMaxUnverifiedSize) {
          out->SetStringPiece(kCertificateTag, compressed);
          out->SetStringPiece(kPROF, signature);
        }
        break;
      }
    }

    return QUIC_NO_ERROR;
  }

  const QuicTag* their_aeads;
  const QuicTag* their_key_exchanges;
  size_t num_their_aeads, num_their_key_exchanges;
  if (client_hello.GetTaglist(kAEAD, &their_aeads,
                              &num_their_aeads) != QUIC_NO_ERROR ||
      client_hello.GetTaglist(kKEXS, &their_key_exchanges,
                              &num_their_key_exchanges) != QUIC_NO_ERROR ||
      num_their_aeads != 1 ||
      num_their_key_exchanges != 1) {
    *error_details = "Missing or invalid AEAD or KEXS";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  size_t key_exchange_index;
  if (!CryptoUtils::FindMutualTag(config->aead, their_aeads, num_their_aeads,
                                  CryptoUtils::LOCAL_PRIORITY, &params->aead,
                                  NULL) ||
      !CryptoUtils::FindMutualTag(
          config->kexs, their_key_exchanges, num_their_key_exchanges,
          CryptoUtils::LOCAL_PRIORITY, &params->key_exchange,
          &key_exchange_index)) {
    *error_details = "Unsupported AEAD or KEXS";
    return QUIC_CRYPTO_NO_SUPPORT;
  }

  StringPiece public_value;
  if (!client_hello.GetStringPiece(kPUBS, &public_value)) {
    *error_details = "Missing public value";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  const KeyExchange* key_exchange = config->key_exchanges[key_exchange_index];
  if (!key_exchange->CalculateSharedKey(public_value,
                                        &params->initial_premaster_secret)) {
    *error_details = "Invalid public value";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  params->server_config_id = scid.as_string();

  string hkdf_suffix;
  const QuicData& client_hello_serialized = client_hello.GetSerialized();
  hkdf_suffix.reserve(sizeof(guid) + client_hello_serialized.length() +
                      config->serialized.size());
  hkdf_suffix.append(reinterpret_cast<char*>(&guid), sizeof(guid));
  hkdf_suffix.append(client_hello_serialized.data(),
                     client_hello_serialized.length());
  hkdf_suffix.append(config->serialized);

  string hkdf_input;
  size_t label_len = strlen(QuicCryptoConfig::kInitialLabel) + 1;
  hkdf_input.reserve(label_len + hkdf_suffix.size());
  hkdf_input.append(QuicCryptoConfig::kInitialLabel, label_len);
  hkdf_input.append(hkdf_suffix);

  CryptoUtils::DeriveKeys(params->initial_premaster_secret, params->aead,
                          client_nonce, params->server_nonce, hkdf_input,
                          CryptoUtils::SERVER, &params->initial_crypters);

  string forward_secure_public_value;
  if (ephemeral_key_source_.get()) {
    params->forward_secure_premaster_secret =
        ephemeral_key_source_->CalculateForwardSecureKey(
            key_exchange, rand, clock->ApproximateNow(), public_value,
            &forward_secure_public_value);
  } else {
    scoped_ptr<KeyExchange> forward_secure_key_exchange(
        key_exchange->NewKeyPair(rand));
    forward_secure_public_value =
        forward_secure_key_exchange->public_value().as_string();
    if (!forward_secure_key_exchange->CalculateSharedKey(
            public_value, &params->forward_secure_premaster_secret)) {
      *error_details = "Invalid public value";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }
  }

  string forward_secure_hkdf_input;
  label_len = strlen(QuicCryptoConfig::kForwardSecureLabel) + 1;
  forward_secure_hkdf_input.reserve(label_len + hkdf_suffix.size());
  forward_secure_hkdf_input.append(QuicCryptoConfig::kForwardSecureLabel,
                                   label_len);
  forward_secure_hkdf_input.append(hkdf_suffix);

  CryptoUtils::DeriveKeys(params->forward_secure_premaster_secret, params->aead,
                          client_nonce, params->server_nonce,
                          forward_secure_hkdf_input, CryptoUtils::SERVER,
                          &params->forward_secure_crypters);

  out->set_tag(kSHLO);
  out->SetStringPiece(kSourceAddressTokenTag, fresh_source_address_token);
  out->SetStringPiece(kPUBS, forward_secure_public_value);
  return QUIC_NO_ERROR;
}

void QuicCryptoServerConfig::SetProofSource(ProofSource* proof_source) {
  proof_source_.reset(proof_source);
}

void QuicCryptoServerConfig::SetEphemeralKeySource(
    EphemeralKeySource* ephemeral_key_source) {
  ephemeral_key_source_.reset(ephemeral_key_source);
}

string QuicCryptoServerConfig::NewSourceAddressToken(
    const IPEndPoint& ip,
    QuicRandom* rand,
    QuicWallTime now) const {
  SourceAddressToken source_address_token;
  source_address_token.set_ip(ip.ToString());
  source_address_token.set_timestamp(now.ToUNIXSeconds());

  string plaintext = source_address_token.SerializeAsString();
  char nonce[12];
  DCHECK_EQ(sizeof(nonce),
            source_address_token_encrypter_->GetNoncePrefixSize() +
            sizeof(QuicPacketSequenceNumber));
  rand->RandBytes(nonce, sizeof(nonce));

  size_t ciphertext_size =
      source_address_token_encrypter_->GetCiphertextSize(plaintext.size());
  string result;
  result.resize(sizeof(nonce) + ciphertext_size);
  memcpy(&result[0], &nonce, sizeof(nonce));

  if (!source_address_token_encrypter_->Encrypt(
          StringPiece(nonce, sizeof(nonce)), StringPiece(), plaintext,
          reinterpret_cast<unsigned char*>(&result[sizeof(nonce)]))) {
    DCHECK(false);
    return string();
  }

  return result;
}

bool QuicCryptoServerConfig::ValidateSourceAddressToken(
    StringPiece token,
    const IPEndPoint& ip,
    QuicWallTime now) const {
  char nonce[12];
  DCHECK_EQ(sizeof(nonce),
            source_address_token_encrypter_->GetNoncePrefixSize() +
            sizeof(QuicPacketSequenceNumber));

  if (token.size() <= sizeof(nonce)) {
    return false;
  }
  memcpy(&nonce, token.data(), sizeof(nonce));
  token.remove_prefix(sizeof(nonce));

  unsigned char plaintext_stack[128];
  scoped_ptr<unsigned char[]> plaintext_heap;
  unsigned char* plaintext;
  if (token.size() <= sizeof(plaintext_stack)) {
    plaintext = plaintext_stack;
  } else {
    plaintext_heap.reset(new unsigned char[token.size()]);
    plaintext = plaintext_heap.get();
  }
  size_t plaintext_length;

  if (!source_address_token_decrypter_->Decrypt(
          StringPiece(nonce, sizeof(nonce)), StringPiece(), token, plaintext,
          &plaintext_length)) {
    return false;
  }

  SourceAddressToken source_address_token;
  if (!source_address_token.ParseFromArray(plaintext, plaintext_length)) {
    return false;
  }

  if (source_address_token.ip() != ip.ToString()) {
    // It's for a different IP address.
    return false;
  }

  const QuicWallTime timestamp(
      QuicWallTime::FromUNIXSeconds(source_address_token.timestamp()));
  const QuicTime::Delta delta(now.AbsoluteDifference(timestamp));

  // TODO(agl): consider whether and how these magic values should be moved to
  // a config.
  if (now.IsBefore(timestamp) && delta.ToSeconds() > 3600) {
    // We only allow timestamps to be from an hour in the future.
    return false;
  }

  if (now.IsAfter(timestamp) && delta.ToSeconds() > 86400) {
    // We allow one day into the past.
    return false;
  }

  return true;
}

QuicCryptoServerConfig::Config::Config() {}

QuicCryptoServerConfig::Config::~Config() { STLDeleteElements(&key_exchanges); }

}  // namespace net
