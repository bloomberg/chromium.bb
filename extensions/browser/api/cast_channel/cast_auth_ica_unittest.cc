// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_ica.h"

#include <string>

#include "base/base64.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace core_api {

namespace cast_channel {

namespace {

static const net::SHA256HashValue kFingerprintValid1 = {{
    0x52, 0x9D, 0x9C, 0xD6, 0x7F, 0xE5, 0xEB, 0x69, 0x8E, 0x70, 0xDD, 0x26,
    0xD7, 0xD8, 0xF1, 0x26, 0x59, 0xF1, 0xE6, 0xE5, 0x23, 0x48, 0xBF, 0x6A,
    0x5C, 0xF7, 0x16, 0xE1, 0x3F, 0x41, 0x0E, 0x73
}};

static const net::SHA256HashValue kFingerprintValid2 = {{
    0xA2, 0x48, 0xC2, 0xE8, 0x54, 0xE6, 0x56, 0xA5, 0x6D, 0xE8, 0x23, 0x1F,
    0x1E, 0xE1, 0x75, 0x6F, 0xDB, 0xE4, 0x07, 0xF9, 0xFE, 0xD4, 0x65, 0x0D,
    0x60, 0xCC, 0x5A, 0xCB, 0x65, 0x11, 0xC7, 0x20
}};

static const net::SHA256HashValue kFingerprintInvalid = {{
    0x00, 0x9D, 0x9C, 0xD6, 0x7F, 0xE5, 0xEB, 0x69, 0x8E, 0x70, 0xDD, 0x26,
    0xD7, 0xD8, 0xF1, 0x26, 0x59, 0xF1, 0xE6, 0xE5, 0x23, 0x48, 0xBF, 0x6A,
    0x5C, 0xF7, 0x16, 0xE1, 0x3F, 0x41, 0x0E, 0x73
}};

}  // namespace

class CastChannelAuthorityKeysTest : public testing::Test {
 public:
  CastChannelAuthorityKeysTest() {}
  ~CastChannelAuthorityKeysTest() override {}

 protected:
  void ExpectKeysLoaded();
  AuthorityKeyStore authority_keys_store_;
};

void CastChannelAuthorityKeysTest::ExpectKeysLoaded() {
  base::StringPiece key = authority_keys_store_.GetDefaultICAPublicKey();
  EXPECT_FALSE(key.empty());

  key =
      authority_keys_store_.GetICAPublicKeyFromFingerprint(kFingerprintValid1);
  EXPECT_FALSE(key.empty());

  key =
      authority_keys_store_.GetICAPublicKeyFromFingerprint(kFingerprintInvalid);
  EXPECT_TRUE(key.empty());
}

TEST_F(CastChannelAuthorityKeysTest, TestDefaultKeys) {
  ExpectKeysLoaded();
}

TEST_F(CastChannelAuthorityKeysTest, TestInvalidProtobuf) {
  std::string keys = "test";
  EXPECT_EQ(authority_keys_store_.Load(keys), false);

  base::StringPiece key = authority_keys_store_.GetDefaultICAPublicKey();
  EXPECT_TRUE(key.empty());
}

TEST_F(CastChannelAuthorityKeysTest, TestValidProtobuf) {
  std::string keys =
      "CrMCCiBSnZzWf+XraY5w3SbX2PEmWfHm5SNIv2pc9xbhP0EOcxKOAjCCAQoCggEBALwigL"
      "2A9johADuudl41fz3DZFxVlIY0LwWHKM33aYwXs1CnuIL638dDLdZ+q6BvtxNygKRHFcEg"
      "mVDN7BRiCVukmM3SQbY2Tv/oLjIwSoGoQqNsmzNuyrL1U2bgJ1OGGoUepzk/SneO+1RmZv"
      "tYVMBeOcf1UAYL4IrUzuFqVR+LFwDmaaMn5gglaTwSnY0FLNYuojHetFJQ1iBJ3nGg+a0g"
      "QBLx3SXr1ea4NvTWj3/KQ9zXEFvmP1GKhbPz//YDLcsjT5ytGOeTBYysUpr3TOmZer5ufk"
      "0K48YcqZP6OqWRXRy9ZuvMYNyGdMrP+JIcmH1X+mFHnquAt+RIgCqSxRsCAwEAAQqzAgog"
      "okjC6FTmVqVt6CMfHuF1b9vkB/n+1GUNYMxay2URxyASjgIwggEKAoIBAQCwDl4HOt+kX2"
      "j3Icdk27Z27+6Lk/j2G4jhk7cX8BUeflJVdzwCjXtKbNO91sGccsizFc8RwfVGxNUgR/sw"
      "9ORhDGjwXqs3jpvhvIHDcIp41oM0MpwZYuvknO3jZGxBHZzSi0hMI5CVs+dS6gVXzGCzuh"
      "TkugA55EZVdM5ajnpnI9poCvrEhB60xaGianMfbsguL5qeqLEO/Yemj009SwXVNVp0TbyO"
      "gkSW9LWVYE6l3yc9QVwHo7Q1WrOe8gUkys0xWg0mTNTT/VDhNOlMgVgwssd63YGJptQ6OI"
      "QDtzSedz//eAdbmcGyHzVWbjo8DCXhV/aKfknAzIMRNeeRbS5lAgMBAAE=";

  std::string decoded_keys;
  EXPECT_EQ(base::Base64Decode(keys, &decoded_keys), true);
  EXPECT_EQ(authority_keys_store_.Load(decoded_keys), true);

  ExpectKeysLoaded();

  base::StringPiece key =
      authority_keys_store_.GetICAPublicKeyFromFingerprint(kFingerprintValid2);
  EXPECT_FALSE(key.empty());
}

TEST_F(CastChannelAuthorityKeysTest, TestSetTrustedCertificateAuthorities) {
  std::string keys =
      "CrMCCiBSnZzWf+XraY5w3SbX2PEmWfHm5SNIv2pc9xbhP0EOcxKOAjCCAQoCggEBALwigL"
      "2A9johADuudl41fz3DZFxVlIY0LwWHKM33aYwXs1CnuIL638dDLdZ+q6BvtxNygKRHFcEg"
      "mVDN7BRiCVukmM3SQbY2Tv/oLjIwSoGoQqNsmzNuyrL1U2bgJ1OGGoUepzk/SneO+1RmZv"
      "tYVMBeOcf1UAYL4IrUzuFqVR+LFwDmaaMn5gglaTwSnY0FLNYuojHetFJQ1iBJ3nGg+a0g"
      "QBLx3SXr1ea4NvTWj3/KQ9zXEFvmP1GKhbPz//YDLcsjT5ytGOeTBYysUpr3TOmZer5ufk"
      "0K48YcqZP6OqWRXRy9ZuvMYNyGdMrP+JIcmH1X+mFHnquAt+RIgCqSxRsCAwEAAQ==";
  std::string signature =
      "chCUHZKkykcwU8HzU+hm027fUTBL0dqPMtrzppwExQwK9+"
      "XlmCjJswfce2sUUfhR1OL1tyW4hWFwu4JnuQCJ+CvmSmAh2bzRpnuSKzBfgvIDjNOAGUs7"
      "ADaNSSWPLxp+6ko++2Dn4S9HpOt8N1v6gMWqj3Ru5IqFSQPZSvGH2ois6uE50CFayPcjQE"
      "OVZt41noQdFd15RmKTvocoCC5tHNlaikeQ52yi0IScOlad1B1lMhoplW3rWophQaqxMumr"
      "OcHIZ+Y+p858x5f8Pny/kuqUClmFh9B/vF07NsUHwoSL9tA5t5jCY3L5iUc/v7o3oFcW/T"
      "gojKkX2Kg7KQ86QA==";
  EXPECT_FALSE(SetTrustedCertificateAuthorities(keys, "signature"));
  EXPECT_FALSE(SetTrustedCertificateAuthorities("keys", signature));
  EXPECT_FALSE(SetTrustedCertificateAuthorities(keys, signature));
  EXPECT_FALSE(SetTrustedCertificateAuthorities(std::string(), std::string()));

  keys =
      "CrMCCiBSnZzWf+XraY5w3SbX2PEmWfHm5SNIv2pc9xbhP0EOcxKOAjCCAQoCggEBALwigL"
      "2A9johADuudl41fz3DZFxVlIY0LwWHKM33aYwXs1CnuIL638dDLdZ+q6BvtxNygKRHFcEg"
      "mVDN7BRiCVukmM3SQbY2Tv/oLjIwSoGoQqNsmzNuyrL1U2bgJ1OGGoUepzk/SneO+1RmZv"
      "tYVMBeOcf1UAYL4IrUzuFqVR+LFwDmaaMn5gglaTwSnY0FLNYuojHetFJQ1iBJ3nGg+a0g"
      "QBLx3SXr1ea4NvTWj3/KQ9zXEFvmP1GKhbPz//YDLcsjT5ytGOeTBYysUpr3TOmZer5ufk"
      "0K48YcqZP6OqWRXRy9ZuvMYNyGdMrP+JIcmH1X+mFHnquAt+RIgCqSxRsCAwEAAQqzAgog"
      "okjC6FTmVqVt6CMfHuF1b9vkB/n+1GUNYMxay2URxyASjgIwggEKAoIBAQCwDl4HOt+kX2"
      "j3Icdk27Z27+6Lk/j2G4jhk7cX8BUeflJVdzwCjXtKbNO91sGccsizFc8RwfVGxNUgR/sw"
      "9ORhDGjwXqs3jpvhvIHDcIp41oM0MpwZYuvknO3jZGxBHZzSi0hMI5CVs+dS6gVXzGCzuh"
      "TkugA55EZVdM5ajnpnI9poCvrEhB60xaGianMfbsguL5qeqLEO/Yemj009SwXVNVp0TbyO"
      "gkSW9LWVYE6l3yc9QVwHo7Q1WrOe8gUkys0xWg0mTNTT/VDhNOlMgVgwssd63YGJptQ6OI"
      "QDtzSedz//eAdbmcGyHzVWbjo8DCXhV/aKfknAzIMRNeeRbS5lAgMBAAE=";
  signature =
      "o83oku3jP+xjTysNBalqp/ZfJRPLt8R+IUhZMepbARFSRVizLoeFW5XyUwe6lQaC+PFFQH"
      "SZeGZyeeGRpwCJ/lef0xh6SWJlVMWNTk5+z0U84GQdizJP/CTCeHpIwMobN+kyDajgOyfD"
      "DLhktc6LHmSlFGG6J7B8W67oziS8ZFEdrcT9WSXFrjLVyURHjvidZD5iFtuImI6k9R9OoX"
      "LR6SyAwpjdrL+vlHMk3Gol6KQ98YpF0ghHnN3/FFW4ibvIwjmRbp+tUV3h8TRcCOjlXVGp"
      "bzPtNRRlTqfv7Rxm5YXkZMLmJJMZiTs5+o8FMRMTQZT4hRR3DQ+A/jofViyTGA==";
  EXPECT_TRUE(SetTrustedCertificateAuthorities(keys, signature));

  base::StringPiece result = GetDefaultTrustedICAPublicKey();
  EXPECT_FALSE(result.empty());
}

}  // namespace cast_channel

}  // namespace core_api

}  // namespace extensions
