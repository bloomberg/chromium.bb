// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter.h"

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "media/cdm/cdm_file_io.h"
#include "media/cdm/cdm_module.h"
#include "media/cdm/external_clear_key_test_helper.h"
#include "media/cdm/mock_helpers.h"
#include "media/cdm/simple_cdm_allocator.h"
#include "media/media_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

MATCHER(IsNotEmpty, "") {
  return !arg.empty();
}

MATCHER(IsNullTime, "") {
  return arg.is_null();
}

// TODO(jrummell): These tests are a subset of those in aes_decryptor_unittest.
// Refactor aes_decryptor_unittest.cc to handle AesDecryptor directly and
// via CdmAdapter once CdmAdapter supports decrypting functionality. There
// will also be tests that only CdmAdapter supports, like file IO, which
// will need to be handled separately.

namespace media {

// Random key ID used to create a session.
const uint8_t kKeyId[] = {
    // base64 equivalent is AQIDBAUGBwgJCgsMDQ4PEA
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
};

const char kKeyIdAsJWK[] = "{\"kids\": [\"AQIDBAUGBwgJCgsMDQ4PEA\"]}";

const uint8_t kKeyIdAsPssh[] = {
    0x00, 0x00, 0x00, 0x34,                          // size = 52
    'p',  's',  's',  'h',                           // 'pssh'
    0x01,                                            // version = 1
    0x00, 0x00, 0x00,                                // flags
    0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
    0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
    0x00, 0x00, 0x00, 0x01,                          // key count
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,  // key
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x00, 0x00, 0x00, 0x00,  // datasize
};

// Key is 0x0405060708090a0b0c0d0e0f10111213,
// base64 equivalent is BAUGBwgJCgsMDQ4PEBESEw.
const char kKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AQIDBAUGBwgJCgsMDQ4PEA\","
    "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
    "    }"
    "  ],"
    "  \"type\": \"temporary\""
    "}";

class CdmAdapterTest : public testing::Test {
 public:
  enum ExpectedResult { SUCCESS, FAILURE };

  CdmAdapterTest() {
    // Enable use of External Clear Key CDM.
    scoped_feature_list_.InitWithFeatures(
        {media::kExternalClearKeyForTesting,
         media::kSupportExperimentalCdmInterface},
        {});

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
    CdmModule::GetInstance()->Initialize(helper_.LibraryPath(), {});
#else
    CdmModule::GetInstance()->Initialize(helper_.LibraryPath());
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  }

  ~CdmAdapterTest() override { CdmModule::ResetInstanceForTesting(); }

 protected:
  // Initializes the adapter. |expected_result| tests that the call succeeds
  // or generates an error.
  void InitializeAndExpect(ExpectedResult expected_result) {
    CdmConfig cdm_config;  // default settings of false are sufficient.
    std::unique_ptr<CdmAllocator> allocator(new SimpleCdmAllocator());
    std::unique_ptr<CdmAuxiliaryHelper> cdm_helper(
        new MockCdmAuxiliaryHelper(std::move(allocator)));
    CdmAdapter::Create(helper_.KeySystemName(), cdm_config,
                       std::move(cdm_helper),
                       base::Bind(&MockCdmClient::OnSessionMessage,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&MockCdmClient::OnSessionClosed,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&MockCdmClient::OnSessionKeysChange,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&MockCdmClient::OnSessionExpirationUpdate,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&CdmAdapterTest::OnCdmCreated,
                                  base::Unretained(this), expected_result));
    RunUntilIdle();
  }

  // Creates a new session using |key_id|. |session_id_| will be set
  // when the promise is resolved. |expected_result| tests that
  // CreateSessionAndGenerateRequest() succeeds or generates an error.
  void CreateSessionAndExpect(EmeInitDataType data_type,
                              const std::vector<uint8_t>& key_id,
                              ExpectedResult expected_result) {
    DCHECK(!key_id.empty());

    if (expected_result == SUCCESS) {
      EXPECT_CALL(cdm_client_, OnSessionMessage(IsNotEmpty(), _, _));
    }

    adapter_->CreateSessionAndGenerateRequest(
        CdmSessionType::TEMPORARY_SESSION, data_type, key_id,
        CreateSessionPromise(expected_result));
    RunUntilIdle();
  }

  // Loads the session specified by |session_id|. |expected_result| tests
  // that LoadSession() succeeds or generates an error.
  void LoadSessionAndExpect(const std::string& session_id,
                            ExpectedResult expected_result) {
    DCHECK(!session_id.empty());
    ASSERT_EQ(expected_result, FAILURE) << "LoadSession not supported.";

    adapter_->LoadSession(CdmSessionType::TEMPORARY_SESSION, session_id,
                          CreateSessionPromise(expected_result));
    RunUntilIdle();
  }

  // Updates the session specified by |session_id| with |key|. |expected_result|
  // tests that the update succeeds or generates an error. |new_key_expected|
  // is the expected parameter when the SessionKeysChange event happens.
  void UpdateSessionAndExpect(std::string session_id,
                              const std::string& key,
                              ExpectedResult expected_result,
                              bool new_key_expected) {
    DCHECK(!key.empty());

    if (expected_result == SUCCESS) {
      EXPECT_CALL(cdm_client_,
                  OnSessionKeysChangeCalled(session_id, new_key_expected));
      EXPECT_CALL(cdm_client_,
                  OnSessionExpirationUpdate(session_id, IsNullTime()));
    } else {
      EXPECT_CALL(cdm_client_, OnSessionKeysChangeCalled(_, _)).Times(0);
      EXPECT_CALL(cdm_client_, OnSessionExpirationUpdate(_, _)).Times(0);
    }

    adapter_->UpdateSession(session_id,
                            std::vector<uint8_t>(key.begin(), key.end()),
                            CreatePromise(expected_result));
    RunUntilIdle();
  }

  std::string SessionId() { return session_id_; }

 private:
  void OnCdmCreated(ExpectedResult expected_result,
                    const scoped_refptr<ContentDecryptionModule>& cdm,
                    const std::string& error_message) {
    if (cdm) {
      ASSERT_EQ(expected_result, SUCCESS)
          << "CDM creation succeeded unexpectedly.";
      adapter_ = cdm;
    } else {
      ASSERT_EQ(expected_result, FAILURE) << error_message;
    }
  }

  // Create a promise. |expected_result| is used to indicate how the promise
  // should be fulfilled.
  std::unique_ptr<SimpleCdmPromise> CreatePromise(
      ExpectedResult expected_result) {
    if (expected_result == SUCCESS) {
      EXPECT_CALL(*this, OnResolve());
    } else {
      EXPECT_CALL(*this, OnReject(_, _, IsNotEmpty()));
    }

    std::unique_ptr<SimpleCdmPromise> promise(new CdmCallbackPromise<>(
        base::Bind(&CdmAdapterTest::OnResolve, base::Unretained(this)),
        base::Bind(&CdmAdapterTest::OnReject, base::Unretained(this))));
    return promise;
  }

  // Create a promise to be used when a new session is created.
  // |expected_result| is used to indicate how the promise should be fulfilled.
  std::unique_ptr<NewSessionCdmPromise> CreateSessionPromise(
      ExpectedResult expected_result) {
    if (expected_result == SUCCESS) {
      EXPECT_CALL(*this, OnResolveWithSession(_))
          .WillOnce(SaveArg<0>(&session_id_));
    } else {
      EXPECT_CALL(*this, OnReject(_, _, IsNotEmpty()));
    }

    std::unique_ptr<NewSessionCdmPromise> promise(
        new CdmCallbackPromise<std::string>(
            base::Bind(&CdmAdapterTest::OnResolveWithSession,
                       base::Unretained(this)),
            base::Bind(&CdmAdapterTest::OnReject, base::Unretained(this))));
    return promise;
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  // Methods used for promise resolved/rejected.
  MOCK_METHOD0(OnResolve, void());
  MOCK_METHOD1(OnResolveWithSession, void(const std::string& session_id));
  MOCK_METHOD3(OnReject,
               void(CdmPromise::Exception exception_code,
                    uint32_t system_code,
                    const std::string& error_message));

  StrictMock<MockCdmClient> cdm_client_;

  // Helper class to load/unload External Clear Key Library.
  ExternalClearKeyTestHelper helper_;

  // Keep track of the loaded CDM.
  scoped_refptr<ContentDecryptionModule> adapter_;

  // |session_id_| is the latest result of calling CreateSession().
  std::string session_id_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CdmAdapterTest);
};

TEST_F(CdmAdapterTest, Initialize) {
  InitializeAndExpect(SUCCESS);
}

TEST_F(CdmAdapterTest, BadLibraryPath) {
  CdmModule::ResetInstanceForTesting();

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  CdmModule::GetInstance()->Initialize(
      base::FilePath(FILE_PATH_LITERAL("no_library_here")), {});
#else
  CdmModule::GetInstance()->Initialize(
      base::FilePath(FILE_PATH_LITERAL("no_library_here")));
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  InitializeAndExpect(FAILURE);
}

TEST_F(CdmAdapterTest, CreateWebmSession) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);
}

TEST_F(CdmAdapterTest, CreateKeyIdsSession) {
  InitializeAndExpect(SUCCESS);

  // Don't include the trailing /0 from the string in the data passed in.
  std::vector<uint8_t> key_id(kKeyIdAsJWK,
                              kKeyIdAsJWK + arraysize(kKeyIdAsJWK) - 1);
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, SUCCESS);
}

TEST_F(CdmAdapterTest, CreateCencSession) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyIdAsPssh,
                              kKeyIdAsPssh + arraysize(kKeyIdAsPssh));
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  CreateSessionAndExpect(EmeInitDataType::CENC, key_id, SUCCESS);
#else
  CreateSessionAndExpect(EmeInitDataType::CENC, key_id, FAILURE);
#endif
}

TEST_F(CdmAdapterTest, CreateSessionWithBadData) {
  InitializeAndExpect(SUCCESS);

  // Use |kKeyId| but specify KEYIDS format.
  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, FAILURE);
}

TEST_F(CdmAdapterTest, LoadSession) {
  InitializeAndExpect(SUCCESS);

  // LoadSession() is not supported by AesDecryptor.
  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, FAILURE);
}

TEST_F(CdmAdapterTest, UpdateSession) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);

  UpdateSessionAndExpect(SessionId(), kKeyAsJWK, SUCCESS, true);
}

TEST_F(CdmAdapterTest, UpdateSessionWithBadData) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);

  UpdateSessionAndExpect(SessionId(), "random data", FAILURE, true);
}

}  // namespace media
