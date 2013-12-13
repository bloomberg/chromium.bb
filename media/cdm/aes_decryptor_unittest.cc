// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/mock_filters.h"
#include "media/cdm/aes_decryptor.h"
#include "media/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrNe;

MATCHER(IsEmpty, "") { return arg.empty(); }

namespace media {

const uint8 kOriginalData[] = "Original subsample data.";
const int kOriginalDataSize = 24;

// In the examples below, 'k'(key) has to be 16 bytes, and will always require
// 2 bytes of padding. 'kid'(keyid) is variable length, and may require 0, 1,
// or 2 bytes of padding.

const uint8 kKeyId[] = {
    // base64 equivalent is AAECAw
    0x00, 0x01, 0x02, 0x03
};

// Key is 0x0405060708090a0b0c0d0e0f10111213,
// base64 equivalent is BAUGBwgJCgsMDQ4PEBESEw.
const char kKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
    "    }"
    "  ]"
    "}";

// Same kid as kKeyAsJWK, key to decrypt kEncryptedData2
const char kKeyAlternateAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
    "    }"
    "  ]"
    "}";

const char kWrongKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"7u7u7u7u7u7u7u7u7u7u7g\""
    "    }"
    "  ]"
    "}";

const char kWrongSizedKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"AAECAw\""
    "    }"
    "  ]"
    "}";

const uint8 kIv[] = {
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// kOriginalData encrypted with kKey and kIv but without any subsamples (or
// equivalently using kSubsampleEntriesCypherOnly).
const uint8 kEncryptedData[] = {
  0x2f, 0x03, 0x09, 0xef, 0x71, 0xaf, 0x31, 0x16,
  0xfa, 0x9d, 0x18, 0x43, 0x1e, 0x96, 0x71, 0xb5,
  0xbf, 0xf5, 0x30, 0x53, 0x9a, 0x20, 0xdf, 0x95
};

// kOriginalData encrypted with kSubsampleKey and kSubsampleIv using
// kSubsampleEntriesNormal.
const uint8 kSubsampleEncryptedData[] = {
  0x4f, 0x72, 0x09, 0x16, 0x09, 0xe6, 0x79, 0xad,
  0x70, 0x73, 0x75, 0x62, 0x09, 0xbb, 0x83, 0x1d,
  0x4d, 0x08, 0xd7, 0x78, 0xa4, 0xa7, 0xf1, 0x2e
};

const uint8 kOriginalData2[] = "Changed Original data.";

const uint8 kIv2[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8 kKeyId2[] = {
    // base64 equivalent is AAECAwQFBgcICQoLDA0ODxAREhM=
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13
};

const char kKey2AsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
    "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
    "    }"
    "  ]"
    "}";

// 'k' in bytes is x14x15x16x17x18x19x1ax1bx1cx1dx1ex1fx20x21x22x23

const uint8 kEncryptedData2[] = {
  0x57, 0x66, 0xf4, 0x12, 0x1a, 0xed, 0xb5, 0x79,
  0x1c, 0x8e, 0x25, 0xd7, 0x17, 0xe7, 0x5e, 0x16,
  0xe3, 0x40, 0x08, 0x27, 0x11, 0xe9
};

// Subsample entries for testing. The sum of |cypher_bytes| and |clear_bytes| of
// all entries must be equal to kOriginalDataSize to make the subsample entries
// valid.

const SubsampleEntry kSubsampleEntriesNormal[] = {
  { 2, 7 },
  { 3, 11 },
  { 1, 0 }
};

const SubsampleEntry kSubsampleEntriesWrongSize[] = {
  { 3, 6 }, // This entry doesn't match the correct entry.
  { 3, 11 },
  { 1, 0 }
};

const SubsampleEntry kSubsampleEntriesInvalidTotalSize[] = {
  { 1, 1000 }, // This entry is too large.
  { 3, 11 },
  { 1, 0 }
};

const SubsampleEntry kSubsampleEntriesClearOnly[] = {
  { 7, 0 },
  { 8, 0 },
  { 9, 0 }
};

const SubsampleEntry kSubsampleEntriesCypherOnly[] = {
  { 0, 6 },
  { 0, 8 },
  { 0, 10 }
};

static scoped_refptr<DecoderBuffer> CreateEncryptedBuffer(
    const std::vector<uint8>& data,
    const std::vector<uint8>& key_id,
    const std::vector<uint8>& iv,
    int offset,
    const std::vector<SubsampleEntry>& subsample_entries) {
  DCHECK(!data.empty());
  int padded_size = offset + data.size();
  scoped_refptr<DecoderBuffer> encrypted_buffer(new DecoderBuffer(padded_size));
  memcpy(encrypted_buffer->writable_data() + offset, &data[0], data.size());
  CHECK(encrypted_buffer.get());
  std::string key_id_string(
      reinterpret_cast<const char*>(key_id.empty() ? NULL : &key_id[0]),
      key_id.size());
  std::string iv_string(
      reinterpret_cast<const char*>(iv.empty() ? NULL : &iv[0]), iv.size());
  encrypted_buffer->set_decrypt_config(scoped_ptr<DecryptConfig>(
      new DecryptConfig(key_id_string, iv_string, offset, subsample_entries)));
  return encrypted_buffer;
}

class AesDecryptorTest : public testing::Test {
 public:
  AesDecryptorTest()
      : decryptor_(base::Bind(&AesDecryptorTest::OnSessionCreated,
                              base::Unretained(this)),
                   base::Bind(&AesDecryptorTest::OnSessionMessage,
                              base::Unretained(this)),
                   base::Bind(&AesDecryptorTest::OnSessionReady,
                              base::Unretained(this)),
                   base::Bind(&AesDecryptorTest::OnSessionClosed,
                              base::Unretained(this)),
                   base::Bind(&AesDecryptorTest::OnSessionError,
                              base::Unretained(this))),
        decrypt_cb_(base::Bind(&AesDecryptorTest::BufferDecrypted,
                               base::Unretained(this))),
        original_data_(kOriginalData, kOriginalData + kOriginalDataSize),
        encrypted_data_(kEncryptedData,
                        kEncryptedData + arraysize(kEncryptedData)),
        subsample_encrypted_data_(
            kSubsampleEncryptedData,
            kSubsampleEncryptedData + arraysize(kSubsampleEncryptedData)),
        key_id_(kKeyId, kKeyId + arraysize(kKeyId)),
        iv_(kIv, kIv + arraysize(kIv)),
        normal_subsample_entries_(
            kSubsampleEntriesNormal,
            kSubsampleEntriesNormal + arraysize(kSubsampleEntriesNormal)),
        next_session_id_(1) {
  }

 protected:
  // Creates a new session using |key_id|. Returns the session ID.
  uint32 CreateSession(const std::vector<uint8>& key_id) {
    DCHECK(!key_id.empty());
    uint32 session_id = next_session_id_++;
    EXPECT_CALL(*this, OnSessionCreated(session_id, StrNe(std::string())));
    EXPECT_CALL(*this, OnSessionMessage(session_id, key_id, ""));
    EXPECT_TRUE(decryptor_.CreateSession(
        session_id, std::string(), &key_id[0], key_id.size()));
    return session_id;
  }

  // Releases the session specified by |session_id|.
  void ReleaseSession(uint32 session_id) {
    EXPECT_CALL(*this, OnSessionClosed(session_id));
    decryptor_.ReleaseSession(session_id);
  }

  enum UpdateSessionExpectation {
    SESSION_READY,
    SESSION_ERROR
  };

  // Updates the session specified by |session_id| with |key|. |result|
  // tests that the update succeeds or generates an error.
  void UpdateSessionAndExpect(uint32 session_id,
                              const std::string& key,
                              UpdateSessionExpectation result) {
    DCHECK(!key.empty());

    switch (result) {
      case SESSION_READY:
        EXPECT_CALL(*this, OnSessionReady(session_id));
        break;
      case SESSION_ERROR:
        EXPECT_CALL(*this,
                    OnSessionError(session_id, MediaKeys::kUnknownError, 0));
        break;
    }

    decryptor_.UpdateSession(
        session_id, reinterpret_cast<const uint8*>(key.c_str()), key.length());
  }

  MOCK_METHOD2(BufferDecrypted, void(Decryptor::Status,
                                     const scoped_refptr<DecoderBuffer>&));

  enum DecryptExpectation {
    SUCCESS,
    DATA_MISMATCH,
    DATA_AND_SIZE_MISMATCH,
    DECRYPT_ERROR,
    NO_KEY
  };

  void DecryptAndExpect(const scoped_refptr<DecoderBuffer>& encrypted,
                        const std::vector<uint8>& plain_text,
                        DecryptExpectation result) {
    scoped_refptr<DecoderBuffer> decrypted;

    switch (result) {
      case SUCCESS:
      case DATA_MISMATCH:
      case DATA_AND_SIZE_MISMATCH:
        EXPECT_CALL(*this, BufferDecrypted(Decryptor::kSuccess, NotNull()))
            .WillOnce(SaveArg<1>(&decrypted));
        break;
      case DECRYPT_ERROR:
        EXPECT_CALL(*this, BufferDecrypted(Decryptor::kError, IsNull()))
            .WillOnce(SaveArg<1>(&decrypted));
        break;
      case NO_KEY:
        EXPECT_CALL(*this, BufferDecrypted(Decryptor::kNoKey, IsNull()))
            .WillOnce(SaveArg<1>(&decrypted));
        break;
    }

    decryptor_.Decrypt(Decryptor::kVideo, encrypted, decrypt_cb_);

    std::vector<uint8> decrypted_text;
    if (decrypted && decrypted->data_size()) {
      decrypted_text.assign(
        decrypted->data(), decrypted->data() + decrypted->data_size());
    }

    switch (result) {
      case SUCCESS:
        EXPECT_EQ(plain_text, decrypted_text);
        break;
      case DATA_MISMATCH:
        EXPECT_EQ(plain_text.size(), decrypted_text.size());
        EXPECT_NE(plain_text, decrypted_text);
        break;
      case DATA_AND_SIZE_MISMATCH:
        EXPECT_NE(plain_text.size(), decrypted_text.size());
        break;
      case DECRYPT_ERROR:
      case NO_KEY:
        EXPECT_TRUE(decrypted_text.empty());
        break;
    }
  }

  MOCK_METHOD2(OnSessionCreated,
               void(uint32 session_id, const std::string& web_session_id));
  MOCK_METHOD3(OnSessionMessage,
               void(uint32 session_id,
                    const std::vector<uint8>& message,
                    const std::string& default_url));
  MOCK_METHOD1(OnSessionReady, void(uint32 session_id));
  MOCK_METHOD1(OnSessionClosed, void(uint32 session_id));
  MOCK_METHOD3(OnSessionError,
               void(uint32 session_id, MediaKeys::KeyError, int system_code));

  AesDecryptor decryptor_;
  AesDecryptor::DecryptCB decrypt_cb_;

  // Constants for testing.
  const std::vector<uint8> original_data_;
  const std::vector<uint8> encrypted_data_;
  const std::vector<uint8> subsample_encrypted_data_;
  const std::vector<uint8> key_id_;
  const std::vector<uint8> iv_;
  const std::vector<SubsampleEntry> normal_subsample_entries_;
  const std::vector<SubsampleEntry> no_subsample_entries_;

  // Generate new session ID every time
  uint32 next_session_id_;
};

TEST_F(AesDecryptorTest, CreateSessionWithNullInitData) {
  uint32 session_id = 8;
  EXPECT_CALL(*this, OnSessionMessage(session_id, IsEmpty(), ""));
  EXPECT_CALL(*this, OnSessionCreated(session_id, StrNe(std::string())));
  EXPECT_TRUE(decryptor_.CreateSession(session_id, std::string(), NULL, 0));
}

TEST_F(AesDecryptorTest, MultipleCreateSession) {
  uint32 session_id1 = 10;
  EXPECT_CALL(*this, OnSessionMessage(session_id1, IsEmpty(), ""));
  EXPECT_CALL(*this, OnSessionCreated(session_id1, StrNe(std::string())));
  EXPECT_TRUE(decryptor_.CreateSession(session_id1, std::string(), NULL, 0));

  uint32 session_id2 = 11;
  EXPECT_CALL(*this, OnSessionMessage(session_id2, IsEmpty(), ""));
  EXPECT_CALL(*this, OnSessionCreated(session_id2, StrNe(std::string())));
  EXPECT_TRUE(decryptor_.CreateSession(session_id2, std::string(), NULL, 0));

  uint32 session_id3 = 23;
  EXPECT_CALL(*this, OnSessionMessage(session_id3, IsEmpty(), ""));
  EXPECT_CALL(*this, OnSessionCreated(session_id3, StrNe(std::string())));
  EXPECT_TRUE(decryptor_.CreateSession(session_id3, std::string(), NULL, 0));
}

TEST_F(AesDecryptorTest, NormalDecryption) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_F(AesDecryptorTest, DecryptionWithOffset) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 23, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_F(AesDecryptorTest, UnencryptedFrame) {
  // An empty iv string signals that the frame is unencrypted.
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      original_data_, key_id_, std::vector<uint8>(), 0, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_F(AesDecryptorTest, WrongKey) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kWrongKeyAsJWK, SESSION_READY);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_F(AesDecryptorTest, NoKey) {
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);
  EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kNoKey, IsNull()));
  decryptor_.Decrypt(Decryptor::kVideo, encrypted_buffer, decrypt_cb_);
}

TEST_F(AesDecryptorTest, KeyReplacement) {
  uint32 session_id = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);

  UpdateSessionAndExpect(session_id, kWrongKeyAsJWK, SESSION_READY);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpect(
      encrypted_buffer, original_data_, DATA_MISMATCH));

  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));
}

TEST_F(AesDecryptorTest, WrongSizedKey) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kWrongSizedKeyAsJWK, SESSION_ERROR);
}

TEST_F(AesDecryptorTest, MultipleKeysAndFrames) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 10, no_subsample_entries_);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  UpdateSessionAndExpect(session_id, kKey2AsJWK, SESSION_READY);

  // The first key is still available after we added a second key.
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  // The second key is also available.
  encrypted_buffer = CreateEncryptedBuffer(
      std::vector<uint8>(kEncryptedData2,
                         kEncryptedData2 + arraysize(kEncryptedData2)),
      std::vector<uint8>(kKeyId2, kKeyId2 + arraysize(kKeyId2)),
      std::vector<uint8>(kIv2, kIv2 + arraysize(kIv2)),
      30,
      no_subsample_entries_);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpect(
      encrypted_buffer,
      std::vector<uint8>(kOriginalData2,
                         kOriginalData2 + arraysize(kOriginalData2) - 1),
      SUCCESS));
}

TEST_F(AesDecryptorTest, CorruptedIv) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);

  std::vector<uint8> bad_iv = iv_;
  bad_iv[1]++;

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, bad_iv, 0, no_subsample_entries_);

  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_F(AesDecryptorTest, CorruptedData) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);

  std::vector<uint8> bad_data = encrypted_data_;
  bad_data[1]++;

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      bad_data, key_id_, iv_, 0, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_F(AesDecryptorTest, EncryptedAsUnencryptedFailure) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, std::vector<uint8>(), 0, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_F(AesDecryptorTest, SubsampleDecryption) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_, 0, normal_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

// Ensures noninterference of data offset and subsample mechanisms. We never
// expect to encounter this in the wild, but since the DecryptConfig doesn't
// disallow such a configuration, it should be covered.
TEST_F(AesDecryptorTest, SubsampleDecryptionWithOffset) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_, 23, normal_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_F(AesDecryptorTest, SubsampleWrongSize) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);

  std::vector<SubsampleEntry> subsample_entries_wrong_size(
      kSubsampleEntriesWrongSize,
      kSubsampleEntriesWrongSize + arraysize(kSubsampleEntriesWrongSize));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_, 0, subsample_entries_wrong_size);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_F(AesDecryptorTest, SubsampleInvalidTotalSize) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);

  std::vector<SubsampleEntry> subsample_entries_invalid_total_size(
      kSubsampleEntriesInvalidTotalSize,
      kSubsampleEntriesInvalidTotalSize +
          arraysize(kSubsampleEntriesInvalidTotalSize));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_, 0,
      subsample_entries_invalid_total_size);
  DecryptAndExpect(encrypted_buffer, original_data_, DECRYPT_ERROR);
}

// No cypher bytes in any of the subsamples.
TEST_F(AesDecryptorTest, SubsampleClearBytesOnly) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);

  std::vector<SubsampleEntry> clear_only_subsample_entries(
      kSubsampleEntriesClearOnly,
      kSubsampleEntriesClearOnly + arraysize(kSubsampleEntriesClearOnly));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      original_data_, key_id_, iv_, 0, clear_only_subsample_entries);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

// No clear bytes in any of the subsamples.
TEST_F(AesDecryptorTest, SubsampleCypherBytesOnly) {
  uint32 session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);

  std::vector<SubsampleEntry> cypher_only_subsample_entries(
      kSubsampleEntriesCypherOnly,
      kSubsampleEntriesCypherOnly + arraysize(kSubsampleEntriesCypherOnly));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, cypher_only_subsample_entries);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_F(AesDecryptorTest, ReleaseSession) {
  uint32 session_id = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);

  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  ReleaseSession(session_id);
}

TEST_F(AesDecryptorTest, NoKeyAfterReleaseSession) {
  uint32 session_id = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);

  UpdateSessionAndExpect(session_id, kKeyAsJWK, SESSION_READY);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  ReleaseSession(session_id);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, NO_KEY));
}

TEST_F(AesDecryptorTest, LatestKeyUsed) {
  uint32 session_id1 = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);

  // Add alternate key, buffer should not be decoded properly.
  UpdateSessionAndExpect(session_id1, kKeyAlternateAsJWK, SESSION_READY);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH));

  // Create a second session with a correct key value for key_id_.
  uint32 session_id2 = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id2, kKeyAsJWK, SESSION_READY);

  // Should be able to decode with latest key.
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));
}

TEST_F(AesDecryptorTest, LatestKeyUsedAfterReleaseSession) {
  uint32 session_id1 = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, 0, no_subsample_entries_);
  UpdateSessionAndExpect(session_id1, kKeyAsJWK, SESSION_READY);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  // Create a second session with a different key value for key_id_.
  uint32 session_id2 = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id2, kKeyAlternateAsJWK, SESSION_READY);

  // Should not be able to decode with new key.
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH));

  // Close second session, should revert to original key.
  ReleaseSession(session_id2);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));
}

TEST_F(AesDecryptorTest, JWKKey) {
  uint32 session_id = CreateSession(key_id_);

  // Try a simple JWK key (i.e. not in a set)
  const std::string kJwkSimple =
      "{"
      "  \"kty\": \"oct\","
      "  \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "  \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "}";
  UpdateSessionAndExpect(session_id, kJwkSimple, SESSION_ERROR);

  // Try a key list with multiple entries.
  const std::string kJwksMultipleEntries =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4/QA\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksMultipleEntries, SESSION_READY);

  // Try a key with no spaces and some \n plus additional fields.
  const std::string kJwksNoSpaces =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\",\"alg\":\"A128KW\","
      "\"kid\":\"AAECAwQFBgcICQoLDA0ODxAREhM\",\"k\":\"GawgguFyGrWKav7AX4VKUg"
      "\",\"foo\":\"bar\"}]}\n\n";
  UpdateSessionAndExpect(session_id, kJwksNoSpaces, SESSION_READY);

  // Try some non-ASCII characters.
  UpdateSessionAndExpect(session_id,
                         "This is not ASCII due to \xff\xfe\xfd in it.",
                         SESSION_ERROR);

  // Try a badly formatted key. Assume that the JSON parser is fully tested,
  // so we won't try a lot of combinations. However, need a test to ensure
  // that the code doesn't crash if invalid JSON received.
  UpdateSessionAndExpect(session_id, "This is not a JSON key.", SESSION_ERROR);

  // Try passing some valid JSON that is not a dictionary at the top level.
  UpdateSessionAndExpect(session_id, "40", SESSION_ERROR);

  // Try an empty dictionary.
  UpdateSessionAndExpect(session_id, "{ }", SESSION_ERROR);

  // Try an empty 'keys' dictionary.
  UpdateSessionAndExpect(session_id, "{ \"keys\": [] }", SESSION_ERROR);

  // Try with 'keys' not a dictionary.
  UpdateSessionAndExpect(session_id, "{ \"keys\":\"1\" }", SESSION_ERROR);

  // Try with 'keys' a list of integers.
  UpdateSessionAndExpect(
      session_id, "{ \"keys\": [ 1, 2, 3 ] }", SESSION_ERROR);

  // Try padding(=) at end of 'k' base64 string.
  const std::string kJwksWithPaddedKey =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAw\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw==\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithPaddedKey, SESSION_ERROR);

  // Try padding(=) at end of 'kid' base64 string.
  const std::string kJwksWithPaddedKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAw==\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithPaddedKeyId, SESSION_ERROR);

  // Try a key with invalid base64 encoding.
  const std::string kJwksWithInvalidBase64 =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"!@#$%^&*()\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithInvalidBase64, SESSION_ERROR);

  // Try a 3-byte 'kid' where no base64 padding is required.
  // |kJwksMultipleEntries| above has 2 'kid's that require 1 and 2 padding
  // bytes. Note that 'k' has to be 16 bytes, so it will always require padding.
  const std::string kJwksWithNoPadding =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"Kiss\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithNoPadding, SESSION_READY);

  // Empty key id.
  const std::string kJwksWithEmptyKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithEmptyKeyId, SESSION_ERROR);
  ReleaseSession(session_id);
}

}  // namespace media
