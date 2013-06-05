// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_server_config.h"

#include <stdlib.h>

#include "base/stl_util.h"
#include "crypto/hkdf.h"
#include "crypto/secure_hash.h"
#include "net/quic/crypto/aes_128_gcm_12_decrypter.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/cert_compressor.h"
#include "net/quic/crypto/channel_id.h"
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

QuicCryptoServerConfig::ConfigOptions::ConfigOptions()
    : expiry_time(QuicWallTime::Zero()),
      channel_id_enabled(false) { }

QuicCryptoServerConfig::QuicCryptoServerConfig(
    StringPiece source_address_token_secret,
    QuicRandom* rand)
    : strike_register_lock_(),
      server_nonce_strike_register_lock_(),
      strike_register_max_entries_(1 << 10),
      strike_register_window_secs_(600),
      source_address_token_future_secs_(3600),
      source_address_token_lifetime_secs_(86400),
      server_nonce_strike_register_max_entries_(1 << 10),
      server_nonce_strike_register_window_secs_(120) {
  crypto::HKDF hkdf(source_address_token_secret, StringPiece() /* no salt */,
                    "QUIC source address token key",
                    CryptoSecretBoxer::GetKeySize(),
                    0 /* no fixed IV needed */);
  source_address_token_boxer_.SetKey(hkdf.server_write_key());

  // Generate a random key and orbit for server nonces.
  rand->RandBytes(server_nonce_orbit_, sizeof(server_nonce_orbit_));
  const size_t key_size = server_nonce_boxer_.GetKeySize();
  scoped_ptr<uint8[]> key_bytes(new uint8[key_size]);
  rand->RandBytes(key_bytes.get(), key_size);

  server_nonce_boxer_.SetKey(
      StringPiece(reinterpret_cast<char*>(key_bytes.get()), key_size));
}

QuicCryptoServerConfig::~QuicCryptoServerConfig() {
  STLDeleteValues(&configs_);
}

// static
QuicServerConfigProtobuf* QuicCryptoServerConfig::DefaultConfig(
    QuicRandom* rand,
    const QuicClock* clock,
    const ConfigOptions& options) {
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

  if (options.expiry_time.IsZero()) {
    const QuicWallTime now = clock->WallNow();
    const QuicWallTime expiry = now.Add(QuicTime::Delta::FromSeconds(
        60 * 60 * 24 * 180 /* 180 days, ~six months */));
    const uint64 expiry_seconds = expiry.ToUNIXSeconds();
    msg.SetValue(kEXPY, expiry_seconds);
  } else {
    msg.SetValue(kEXPY, options.expiry_time.ToUNIXSeconds());
  }

  char scid_bytes[16];
  rand->RandBytes(scid_bytes, sizeof(scid_bytes));
  msg.SetStringPiece(kSCID, StringPiece(scid_bytes, sizeof(scid_bytes)));

  char orbit_bytes[kOrbitSize];
  rand->RandBytes(orbit_bytes, sizeof(orbit_bytes));
  msg.SetStringPiece(kORBT, StringPiece(orbit_bytes, sizeof(orbit_bytes)));

  if (options.channel_id_enabled) {
    msg.SetTaglist(kPDMD, kCHID, 0);
  }

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

  const QuicTag* proof_demand_tags;
  size_t num_proof_demand_tags;
  if (msg->GetTaglist(kPDMD, &proof_demand_tags, &num_proof_demand_tags) ==
      QUIC_NO_ERROR) {
    for (size_t i = 0; i < num_proof_demand_tags; i++) {
      if (proof_demand_tags[i] == kCHID) {
        config->channel_id_enabled = true;
        break;
      }
    }
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

  // FIXME(agl): this is mismatched with |DefaultConfig|, which generates a
  // random id.
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
    const ConfigOptions& options) {
  scoped_ptr<QuicServerConfigProtobuf> config(
      DefaultConfig(rand, clock, options));
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

  if (configs_.empty()) {
    *error_details = "No configurations loaded";
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }

  // FIXME(agl): we should use the client's SCID, not just the active config.
  map<ServerConfigID, Config*>::const_iterator it =
      configs_.find(active_config_);
  if (it == configs_.end()) {
    *error_details = "No valid server config loaded";
    return QUIC_CRYPTO_INTERNAL_ERROR;
  }
  const Config* const config(it->second);

  if (client_hello.size() < kClientHelloMinimumSize) {
    *error_details = "Client hello too small";
    return QUIC_CRYPTO_INVALID_VALUE_LENGTH;
  }

  const QuicWallTime now = clock->WallNow();
  bool valid_source_address_token = false;
  StringPiece srct;
  if (client_hello.GetStringPiece(kSourceAddressTokenTag, &srct) &&
      ValidateSourceAddressToken(srct, client_ip, now)) {
    valid_source_address_token = true;
  }

  StringPiece sni;
  if (client_hello.GetStringPiece(kSNI, &sni) &&
      !CryptoUtils::IsValidSNI(sni)) {
    *error_details = "Invalid SNI name";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  // The client nonce is used first to try and establish uniqueness.
  bool unique_by_strike_register = false;

  StringPiece client_nonce;
  bool client_nonce_well_formed = false;
  if (client_hello.GetStringPiece(kNONC, &client_nonce) &&
      client_nonce.size() == kNonceSize) {
    client_nonce_well_formed = true;
    base::AutoLock auto_lock(strike_register_lock_);

    if (strike_register_.get() == NULL) {
      strike_register_.reset(new StrikeRegister(
          strike_register_max_entries_,
          static_cast<uint32>(now.ToUNIXSeconds()),
          strike_register_window_secs_,
          config->orbit,
          StrikeRegister::DENY_REQUESTS_AT_STARTUP));
    }
    unique_by_strike_register = strike_register_->Insert(
        reinterpret_cast<const uint8*>(client_nonce.data()),
        static_cast<uint32>(now.ToUNIXSeconds()));
  }

  StringPiece server_nonce;
  client_hello.GetStringPiece(kServerNonceTag, &server_nonce);

  // If the client nonce didn't establish uniqueness then an echoed server
  // nonce may.
  bool unique_by_server_nonce = false;
  if (!unique_by_strike_register && !server_nonce.empty()) {
    unique_by_server_nonce = ValidateServerNonce(server_nonce, now);
  }

  out->Clear();

  StringPiece scid;
  if (!client_hello.GetStringPiece(kSCID, &scid) ||
      scid.as_string() != config->id ||
      !valid_source_address_token ||
      !client_nonce_well_formed ||
      (!unique_by_strike_register &&
       !unique_by_server_nonce)) {
    // If the client didn't provide a server config ID, or gave the wrong one,
    // then the handshake cannot possibly complete. We reject the handshake and
    // give the client enough information to do better next time.
    out->set_tag(kREJ);
    out->SetStringPiece(kSCFG, config->serialized);
    out->SetStringPiece(kSourceAddressTokenTag,
                        NewSourceAddressToken(client_ip, rand, now));
    out->SetStringPiece(kServerNonceTag, NewServerNonce(rand, now));

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
            config->common_cert_sets);

        // kMaxUnverifiedSize is the number of bytes that the certificate chain
        // and signature can consume before we will demand a valid
        // source-address token.
        // TODO(agl): make this configurable.
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
  if (!QuicUtils::FindMutualTag(config->aead, their_aeads, num_their_aeads,
                                QuicUtils::LOCAL_PRIORITY, &params->aead,
                                NULL) ||
      !QuicUtils::FindMutualTag(
          config->kexs, their_key_exchanges, num_their_key_exchanges,
          QuicUtils::LOCAL_PRIORITY, &params->key_exchange,
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
  if (!sni.empty()) {
    scoped_ptr<char[]> sni_tmp(new char[sni.length() + 1]);
    memcpy(sni_tmp.get(), sni.data(), sni.length());
    sni_tmp[sni.length()] = 0;
    params->sni = CryptoUtils::NormalizeHostname(sni_tmp.get());
  }

  string hkdf_suffix;
  const QuicData& client_hello_serialized = client_hello.GetSerialized();
  hkdf_suffix.reserve(sizeof(guid) + client_hello_serialized.length() +
                      config->serialized.size());
  hkdf_suffix.append(reinterpret_cast<char*>(&guid), sizeof(guid));
  hkdf_suffix.append(client_hello_serialized.data(),
                     client_hello_serialized.length());
  hkdf_suffix.append(config->serialized);

  StringPiece cetv_ciphertext;
  if (config->channel_id_enabled &&
      client_hello.GetStringPiece(kCETV, &cetv_ciphertext)) {
    CryptoHandshakeMessage client_hello_copy(client_hello);
    client_hello_copy.Erase(kCETV);
    client_hello_copy.Erase(kPAD);

    const QuicData& client_hello_serialized = client_hello_copy.GetSerialized();
    string hkdf_input;
    hkdf_input.append(QuicCryptoConfig::kCETVLabel,
                      strlen(QuicCryptoConfig::kCETVLabel) + 1);
    hkdf_input.append(reinterpret_cast<char*>(&guid), sizeof(guid));
    hkdf_input.append(client_hello_serialized.data(),
                      client_hello_serialized.length());
    hkdf_input.append(config->serialized);

    CrypterPair crypters;
    CryptoUtils::DeriveKeys(params->initial_premaster_secret, params->aead,
                            client_nonce, server_nonce, hkdf_input,
                            CryptoUtils::SERVER, &crypters);

    scoped_ptr<QuicData> cetv_plaintext(crypters.decrypter->DecryptPacket(
        0 /* sequence number */, StringPiece() /* associated data */,
        cetv_ciphertext));
    if (!cetv_plaintext.get()) {
      *error_details = "CETV decryption failure";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    scoped_ptr<CryptoHandshakeMessage> cetv(CryptoFramer::ParseMessage(
        cetv_plaintext->AsStringPiece()));
    if (!cetv.get()) {
      *error_details = "CETV parse error";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    StringPiece key, signature;
    if (cetv->GetStringPiece(kCIDK, &key) &&
        cetv->GetStringPiece(kCIDS, &signature)) {
      if (!ChannelIDVerifier::Verify(key, hkdf_input, signature)) {
        *error_details = "ChannelID signature failure";
        return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
      }

      params->channel_id = key.as_string();
    }
  }

  string hkdf_input;
  size_t label_len = strlen(QuicCryptoConfig::kInitialLabel) + 1;
  hkdf_input.reserve(label_len + hkdf_suffix.size());
  hkdf_input.append(QuicCryptoConfig::kInitialLabel, label_len);
  hkdf_input.append(hkdf_suffix);

  CryptoUtils::DeriveKeys(params->initial_premaster_secret, params->aead,
                          client_nonce, server_nonce, hkdf_input,
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
                          client_nonce, server_nonce, forward_secure_hkdf_input,
                          CryptoUtils::SERVER,
                          &params->forward_secure_crypters);

  out->set_tag(kSHLO);
  out->SetStringPiece(kSourceAddressTokenTag,
                      NewSourceAddressToken(client_ip, rand, now));
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

void QuicCryptoServerConfig::set_strike_register_max_entries(
    uint32 max_entries) {
  DCHECK(!strike_register_.get());
  strike_register_max_entries_ = max_entries;
}

void QuicCryptoServerConfig::set_strike_register_window_secs(
    uint32 window_secs) {
  DCHECK(!strike_register_.get());
  strike_register_window_secs_ = window_secs;
}

void QuicCryptoServerConfig::set_source_address_token_future_secs(
    uint32 future_secs) {
  source_address_token_future_secs_ = future_secs;
}

void QuicCryptoServerConfig::set_source_address_token_lifetime_secs(
    uint32 lifetime_secs) {
  source_address_token_lifetime_secs_ = lifetime_secs;
}

void QuicCryptoServerConfig::set_server_nonce_strike_register_max_entries(
    uint32 max_entries) {
  DCHECK(!server_nonce_strike_register_.get());
  server_nonce_strike_register_max_entries_ = max_entries;
}

void QuicCryptoServerConfig::set_server_nonce_strike_register_window_secs(
    uint32 window_secs) {
  DCHECK(!server_nonce_strike_register_.get());
  server_nonce_strike_register_window_secs_ = window_secs;
}

string QuicCryptoServerConfig::NewSourceAddressToken(
    const IPEndPoint& ip,
    QuicRandom* rand,
    QuicWallTime now) const {
  SourceAddressToken source_address_token;
  source_address_token.set_ip(ip.ToString());
  source_address_token.set_timestamp(now.ToUNIXSeconds());

  return source_address_token_boxer_.Box(
      rand, source_address_token.SerializeAsString());
}

bool QuicCryptoServerConfig::ValidateSourceAddressToken(
    StringPiece token,
    const IPEndPoint& ip,
    QuicWallTime now) const {
  string storage;
  StringPiece plaintext;
  if (!source_address_token_boxer_.Unbox(token, &storage, &plaintext)) {
    return false;
  }

  SourceAddressToken source_address_token;
  if (!source_address_token.ParseFromArray(plaintext.data(),
                                           plaintext.size())) {
    return false;
  }

  if (source_address_token.ip() != ip.ToString()) {
    // It's for a different IP address.
    return false;
  }

  const QuicWallTime timestamp(
      QuicWallTime::FromUNIXSeconds(source_address_token.timestamp()));
  const QuicTime::Delta delta(now.AbsoluteDifference(timestamp));

  if (now.IsBefore(timestamp) &&
      delta.ToSeconds() > source_address_token_future_secs_) {
    return false;
  }

  if (now.IsAfter(timestamp) &&
      delta.ToSeconds() > source_address_token_lifetime_secs_) {
    return false;
  }

  return true;
}

// kServerNoncePlaintextSize is the number of bytes in an unencrypted server
// nonce.
static const size_t kServerNoncePlaintextSize =
    4 /* timestamp */ + 20 /* random bytes */;

string QuicCryptoServerConfig::NewServerNonce(QuicRandom* rand,
                                              QuicWallTime now) const {
  const uint32 timestamp = static_cast<uint32>(now.ToUNIXSeconds());

  uint8 server_nonce[kServerNoncePlaintextSize];
  COMPILE_ASSERT(sizeof(server_nonce) > sizeof(timestamp), nonce_too_small);
  server_nonce[0] = static_cast<uint8>(timestamp >> 24);
  server_nonce[1] = static_cast<uint8>(timestamp >> 16);
  server_nonce[2] = static_cast<uint8>(timestamp >> 8);
  server_nonce[3] = static_cast<uint8>(timestamp);
  rand->RandBytes(&server_nonce[sizeof(timestamp)],
                  sizeof(server_nonce) - sizeof(timestamp));

  return server_nonce_boxer_.Box(
      rand,
      StringPiece(reinterpret_cast<char*>(server_nonce), sizeof(server_nonce)));
}

bool QuicCryptoServerConfig::ValidateServerNonce(StringPiece token,
                                                 QuicWallTime now) const {
  string storage;
  StringPiece plaintext;
  if (!server_nonce_boxer_.Unbox(token, &storage, &plaintext)) {
    return false;
  }

  // plaintext contains:
  //   uint32 timestamp
  //   uint8[20] random bytes

  if (plaintext.size() != kServerNoncePlaintextSize) {
    // This should never happen because the value decrypted correctly.
    LOG(DFATAL) << "Seemingly valid server nonce had incorrect length.";
    return false;
  }

  uint8 server_nonce[32];
  memcpy(server_nonce, plaintext.data(), 4);
  memcpy(server_nonce + 4, server_nonce_orbit_, sizeof(server_nonce_orbit_));
  memcpy(server_nonce + 4 + sizeof(server_nonce_orbit_), plaintext.data() + 4,
         20);
  COMPILE_ASSERT(4 + sizeof(server_nonce_orbit_) + 20 == sizeof(server_nonce),
                 bad_nonce_buffer_length);

  bool is_unique;
  {
    base::AutoLock auto_lock(server_nonce_strike_register_lock_);
    if (server_nonce_strike_register_.get() == NULL) {
      server_nonce_strike_register_.reset(new StrikeRegister(
          server_nonce_strike_register_max_entries_,
          static_cast<uint32>(now.ToUNIXSeconds()),
          server_nonce_strike_register_window_secs_, server_nonce_orbit_,
          StrikeRegister::NO_STARTUP_PERIOD_NEEDED));
    }
    is_unique = server_nonce_strike_register_->Insert(
        server_nonce, static_cast<uint32>(now.ToUNIXSeconds()));
  }

  return is_unique;
}

QuicCryptoServerConfig::Config::Config() : channel_id_enabled(false) { }

QuicCryptoServerConfig::Config::~Config() { STLDeleteElements(&key_exchanges); }

}  // namespace net
