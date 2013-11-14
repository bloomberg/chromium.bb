// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/aes_decryptor.h"

#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace media {

uint32 AesDecryptor::next_session_id_ = 1;

enum ClearBytesBufferSel {
  kSrcContainsClearBytes,
  kDstContainsClearBytes
};

typedef std::vector<std::pair<std::string, std::string> > JWKKeys;

static void CopySubsamples(const std::vector<SubsampleEntry>& subsamples,
                           const ClearBytesBufferSel sel,
                           const uint8* src,
                           uint8* dst) {
  for (size_t i = 0; i < subsamples.size(); i++) {
    const SubsampleEntry& subsample = subsamples[i];
    if (sel == kSrcContainsClearBytes) {
      src += subsample.clear_bytes;
    } else {
      dst += subsample.clear_bytes;
    }
    memcpy(dst, src, subsample.cypher_bytes);
    src += subsample.cypher_bytes;
    dst += subsample.cypher_bytes;
  }
}

// Helper to decode a base64 string. EME spec doesn't allow padding characters,
// but base::Base64Decode() requires them. So check that they're not there, and
// then add them before calling base::Base64Decode().
static bool DecodeBase64(std::string encoded_text, std::string* decoded_text) {
  const char base64_padding = '=';

  // TODO(jrummell): Enable this after layout tests have been updated to not
  // include trailing padding characters.
  // if (encoded_text.back() == base64_padding)
  //   return false;

  // Add pad characters so length of |encoded_text| is exactly a multiple of 4.
  size_t num_last_grouping_chars = encoded_text.length() % 4;
  if (num_last_grouping_chars > 0)
    encoded_text.append(4 - num_last_grouping_chars, base64_padding);

  return base::Base64Decode(encoded_text, decoded_text);
}

// Processes a JSON Web Key to extract the key id and key value. Adds the
// id/value pair to |jwk_keys| and returns true on success.
static bool ProcessSymmetricKeyJWK(const DictionaryValue& jwk,
                                   JWKKeys* jwk_keys) {
  // A symmetric keys JWK looks like the following in JSON:
  //   { "kty":"oct",
  //     "kid":"AAECAwQFBgcICQoLDA0ODxAREhM",
  //     "k":"FBUWFxgZGhscHR4fICEiIw" }
  // There may be other properties specified, but they are ignored.
  // Ref: http://tools.ietf.org/html/draft-ietf-jose-json-web-key-14
  // and:
  // http://tools.ietf.org/html/draft-jones-jose-json-private-and-symmetric-key-00

  // Have found a JWK, start by checking that it is a symmetric key.
  std::string type;
  if (!jwk.GetString("kty", &type) || type != "oct") {
    DVLOG(1) << "JWK is not a symmetric key";
    return false;
  }

  // Get the key id and actual key parameters.
  std::string encoded_key_id;
  std::string encoded_key;
  if (!jwk.GetString("kid", &encoded_key_id)) {
    DVLOG(1) << "Missing 'kid' parameter";
    return false;
  }
  if (!jwk.GetString("k", &encoded_key)) {
    DVLOG(1) << "Missing 'k' parameter";
    return false;
  }

  // Key ID and key are base64-encoded strings, so decode them.
  std::string decoded_key_id;
  std::string decoded_key;
  if (!DecodeBase64(encoded_key_id, &decoded_key_id) ||
      decoded_key_id.empty()) {
    DVLOG(1) << "Invalid 'kid' value";
    return false;
  }
  if (!DecodeBase64(encoded_key, &decoded_key) ||
      decoded_key.length() !=
          static_cast<size_t>(DecryptConfig::kDecryptionKeySize)) {
    DVLOG(1) << "Invalid length of 'k' " << decoded_key.length();
    return false;
  }

  // Add the decoded key ID and the decoded key to the list.
  jwk_keys->push_back(std::make_pair(decoded_key_id, decoded_key));
  return true;
}

// Extracts the JSON Web Keys from a JSON Web Key Set. If |input| looks like
// a valid JWK Set, then true is returned and |jwk_keys| is updated to contain
// the list of keys found. Otherwise return false.
static bool ExtractJWKKeys(const std::string& input, JWKKeys* jwk_keys) {
  // TODO(jrummell): The EME spec references a smaller set of allowed ASCII
  // values. Verify with spec that the smaller character set is needed.
  if (!IsStringASCII(input))
    return false;

  scoped_ptr<Value> root(base::JSONReader().ReadToValue(input));
  if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY)
    return false;

  // A JSON Web Key Set looks like the following in JSON:
  //   { "keys": [ JWK1, JWK2, ... ] }
  // (See ProcessSymmetricKeyJWK() for description of JWK.)
  // There may be other properties specified, but they are ignored.
  // Locate the set from the dictionary.
  DictionaryValue* dictionary = static_cast<DictionaryValue*>(root.get());
  ListValue* list_val = NULL;
  if (!dictionary->GetList("keys", &list_val)) {
    DVLOG(1) << "Missing 'keys' parameter or not a list in JWK Set";
    return false;
  }

  // Create a local list of keys, so that |jwk_keys| only gets updated on
  // success.
  JWKKeys local_keys;
  for (size_t i = 0; i < list_val->GetSize(); ++i) {
    DictionaryValue* jwk = NULL;
    if (!list_val->GetDictionary(i, &jwk)) {
      DVLOG(1) << "Unable to access 'keys'[" << i << "] in JWK Set";
      return false;
    }
    if (!ProcessSymmetricKeyJWK(*jwk, &local_keys)) {
      DVLOG(1) << "Error from 'keys'[" << i << "]";
      return false;
    }
  }

  // Successfully processed all JWKs in the set.
  jwk_keys->swap(local_keys);
  return true;
}

// Decrypts |input| using |key|.  Returns a DecoderBuffer with the decrypted
// data if decryption succeeded or NULL if decryption failed.
static scoped_refptr<DecoderBuffer> DecryptData(const DecoderBuffer& input,
                                                crypto::SymmetricKey* key) {
  CHECK(input.data_size());
  CHECK(input.decrypt_config());
  CHECK(key);

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, "")) {
    DVLOG(1) << "Could not initialize decryptor.";
    return NULL;
  }

  DCHECK_EQ(input.decrypt_config()->iv().size(),
            static_cast<size_t>(DecryptConfig::kDecryptionKeySize));
  if (!encryptor.SetCounter(input.decrypt_config()->iv())) {
    DVLOG(1) << "Could not set counter block.";
    return NULL;
  }

  const int data_offset = input.decrypt_config()->data_offset();
  const char* sample =
      reinterpret_cast<const char*>(input.data() + data_offset);
  int sample_size = input.data_size() - data_offset;

  DCHECK_GT(sample_size, 0) << "No sample data to be decrypted.";
  if (sample_size <= 0)
    return NULL;

  if (input.decrypt_config()->subsamples().empty()) {
    std::string decrypted_text;
    base::StringPiece encrypted_text(sample, sample_size);
    if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
      DVLOG(1) << "Could not decrypt data.";
      return NULL;
    }

    // TODO(xhwang): Find a way to avoid this data copy.
    return DecoderBuffer::CopyFrom(
        reinterpret_cast<const uint8*>(decrypted_text.data()),
        decrypted_text.size());
  }

  const std::vector<SubsampleEntry>& subsamples =
      input.decrypt_config()->subsamples();

  int total_clear_size = 0;
  int total_encrypted_size = 0;
  for (size_t i = 0; i < subsamples.size(); i++) {
    total_clear_size += subsamples[i].clear_bytes;
    total_encrypted_size += subsamples[i].cypher_bytes;
  }
  if (total_clear_size + total_encrypted_size != sample_size) {
    DVLOG(1) << "Subsample sizes do not equal input size";
    return NULL;
  }

  // No need to decrypt if there is no encrypted data.
  if (total_encrypted_size <= 0) {
    return DecoderBuffer::CopyFrom(reinterpret_cast<const uint8*>(sample),
                                   sample_size);
  }

  // The encrypted portions of all subsamples must form a contiguous block,
  // such that an encrypted subsample that ends away from a block boundary is
  // immediately followed by the start of the next encrypted subsample. We
  // copy all encrypted subsamples to a contiguous buffer, decrypt them, then
  // copy the decrypted bytes over the encrypted bytes in the output.
  // TODO(strobe): attempt to reduce number of memory copies
  scoped_ptr<uint8[]> encrypted_bytes(new uint8[total_encrypted_size]);
  CopySubsamples(subsamples, kSrcContainsClearBytes,
                 reinterpret_cast<const uint8*>(sample), encrypted_bytes.get());

  base::StringPiece encrypted_text(
      reinterpret_cast<const char*>(encrypted_bytes.get()),
      total_encrypted_size);
  std::string decrypted_text;
  if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
    DVLOG(1) << "Could not decrypt data.";
    return NULL;
  }
  DCHECK_EQ(decrypted_text.size(), encrypted_text.size());

  scoped_refptr<DecoderBuffer> output = DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(sample), sample_size);
  CopySubsamples(subsamples, kDstContainsClearBytes,
                 reinterpret_cast<const uint8*>(decrypted_text.data()),
                 output->writable_data());
  return output;
}

AesDecryptor::AesDecryptor(const KeyAddedCB& key_added_cb,
                           const KeyErrorCB& key_error_cb,
                           const KeyMessageCB& key_message_cb,
                           const SetSessionIdCB& set_session_id_cb)
    : key_added_cb_(key_added_cb),
      key_error_cb_(key_error_cb),
      key_message_cb_(key_message_cb),
      set_session_id_cb_(set_session_id_cb) {}

AesDecryptor::~AesDecryptor() {
  STLDeleteValues(&key_map_);
}

bool AesDecryptor::GenerateKeyRequest(uint32 reference_id,
                                      const std::string& type,
                                      const uint8* init_data,
                                      int init_data_length) {
  std::string session_id_string(base::UintToString(next_session_id_++));

  // For now, the AesDecryptor does not care about |type|;
  // just fire the event with the |init_data| as the request.
  std::vector<uint8> message;
  if (init_data && init_data_length)
    message.assign(init_data, init_data + init_data_length);

  set_session_id_cb_.Run(reference_id, session_id_string);
  key_message_cb_.Run(reference_id, message, std::string());
  return true;
}

void AesDecryptor::AddKey(uint32 reference_id,
                          const uint8* key,
                          int key_length,
                          const uint8* init_data,
                          int init_data_length) {
  CHECK(key);
  CHECK_GT(key_length, 0);

  // AddKey() is called from update(), where the key(s) are passed as a JSON
  // Web Key (JWK) set. Each JWK needs to be a symmetric key ('kty' = "oct"),
  // with 'kid' being the base64-encoded key id, and 'k' being the
  // base64-encoded key.
  //
  // For backwards compatibility with v0.1b of the spec (where |key| is the raw
  // key and |init_data| is the key id), if |key| is not valid JSON, then
  // attempt to process it as a raw key.

  std::string key_string(reinterpret_cast<const char*>(key), key_length);
  JWKKeys jwk_keys;
  if (ExtractJWKKeys(key_string, &jwk_keys)) {
    // Since |key| represents valid JSON, init_data must be empty.
    DCHECK(!init_data);
    DCHECK_EQ(init_data_length, 0);

    // Make sure that at least one key was extracted.
    if (jwk_keys.empty()) {
      key_error_cb_.Run(reference_id, MediaKeys::kUnknownError, 0);
      return;
    }
    for (JWKKeys::iterator it = jwk_keys.begin() ; it != jwk_keys.end(); ++it) {
      if (!AddDecryptionKey(it->first, it->second)) {
        key_error_cb_.Run(reference_id, MediaKeys::kUnknownError, 0);
        return;
      }
    }
  } else {
    // v0.1b backwards compatibility support.
    // TODO(jrummell): Remove this code once v0.1b no longer supported.

    if (key_string.length() !=
        static_cast<size_t>(DecryptConfig::kDecryptionKeySize)) {
      DVLOG(1) << "Invalid key length: " << key_string.length();
      key_error_cb_.Run(reference_id, MediaKeys::kUnknownError, 0);
      return;
    }

    // TODO(xhwang): Fix the decryptor to accept no |init_data|. See
    // http://crbug.com/123265. Until then, ensure a non-empty value is passed.
    static const uint8 kDummyInitData[1] = {0};
    if (!init_data) {
      init_data = kDummyInitData;
      init_data_length = arraysize(kDummyInitData);
    }

    // TODO(xhwang): For now, use |init_data| for key ID. Make this more spec
    // compliant later (http://crbug.com/123262, http://crbug.com/123265).
    std::string key_id_string(reinterpret_cast<const char*>(init_data),
                              init_data_length);
    if (!AddDecryptionKey(key_id_string, key_string)) {
      // Error logged in AddDecryptionKey()
      key_error_cb_.Run(reference_id, MediaKeys::kUnknownError, 0);
      return;
    }
  }

  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();

  key_added_cb_.Run(reference_id);
}

void AesDecryptor::CancelKeyRequest(uint32 reference_id) {
}

Decryptor* AesDecryptor::GetDecryptor() {
  return this;
}

void AesDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                    const NewKeyCB& new_key_cb) {
  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = new_key_cb;
      break;
    case kVideo:
      new_video_key_cb_ = new_key_cb;
      break;
    default:
      NOTREACHED();
  }
}

void AesDecryptor::Decrypt(StreamType stream_type,
                           const scoped_refptr<DecoderBuffer>& encrypted,
                           const DecryptCB& decrypt_cb) {
  CHECK(encrypted->decrypt_config());

  scoped_refptr<DecoderBuffer> decrypted;
  // An empty iv string signals that the frame is unencrypted.
  if (encrypted->decrypt_config()->iv().empty()) {
    int data_offset = encrypted->decrypt_config()->data_offset();
    decrypted = DecoderBuffer::CopyFrom(encrypted->data() + data_offset,
                                        encrypted->data_size() - data_offset);
  } else {
    const std::string& key_id = encrypted->decrypt_config()->key_id();
    DecryptionKey* key = GetKey(key_id);
    if (!key) {
      DVLOG(1) << "Could not find a matching key for the given key ID.";
      decrypt_cb.Run(kNoKey, NULL);
      return;
    }

    crypto::SymmetricKey* decryption_key = key->decryption_key();
    decrypted = DecryptData(*encrypted.get(), decryption_key);
    if (!decrypted.get()) {
      DVLOG(1) << "Decryption failed.";
      decrypt_cb.Run(kError, NULL);
      return;
    }
  }

  decrypted->set_timestamp(encrypted->timestamp());
  decrypted->set_duration(encrypted->duration());
  decrypt_cb.Run(kSuccess, decrypted);
}

void AesDecryptor::CancelDecrypt(StreamType stream_type) {
  // Decrypt() calls the DecryptCB synchronously so there's nothing to cancel.
}

void AesDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support audio decoding.
  init_cb.Run(false);
}

void AesDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support video decoding.
  init_cb.Run(false);
}

void AesDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support audio decoding";
}

void AesDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support video decoding";
}

void AesDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "AesDecryptor does not support audio/video decoding";
}

void AesDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED() << "AesDecryptor does not support audio/video decoding";
}

bool AesDecryptor::AddDecryptionKey(const std::string& key_id,
                                    const std::string& key_string) {
  scoped_ptr<DecryptionKey> decryption_key(new DecryptionKey(key_string));
  if (!decryption_key) {
    DVLOG(1) << "Could not create key.";
    return false;
  }

  if (!decryption_key->Init()) {
    DVLOG(1) << "Could not initialize decryption key.";
    return false;
  }

  base::AutoLock auto_lock(key_map_lock_);
  KeyMap::iterator found = key_map_.find(key_id);
  if (found != key_map_.end()) {
    delete found->second;
    key_map_.erase(found);
  }
  key_map_[key_id] = decryption_key.release();
  return true;
}

AesDecryptor::DecryptionKey* AesDecryptor::GetKey(
    const std::string& key_id) const {
  base::AutoLock auto_lock(key_map_lock_);
  KeyMap::const_iterator found = key_map_.find(key_id);
  if (found == key_map_.end())
    return NULL;

  return found->second;
}

AesDecryptor::DecryptionKey::DecryptionKey(const std::string& secret)
    : secret_(secret) {
}

AesDecryptor::DecryptionKey::~DecryptionKey() {}

bool AesDecryptor::DecryptionKey::Init() {
  CHECK(!secret_.empty());
  decryption_key_.reset(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, secret_));
  if (!decryption_key_)
    return false;
  return true;
}

}  // namespace media
