// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/media_keys.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;
MATCHER(IsNotEmpty, "") {
  return !arg.empty();
}

// TODO(jrummell): These tests are a subset of those in aes_decryptor_unittest.
// Refactor aes_decryptor_unittest.cc to handle AesDecryptor directly and
// via CdmAdapter once CdmAdapter supports decrypting functionality. There
// will also be tests that only CdmAdapter supports, like file IO, which
// will need to be handled separately.

namespace media {

// INITIALIZE_CDM_MODULE is a macro in api/content_decryption_module.h.
// However, we need to pass it as a string to GetFunctionPointer() once it
// is expanded.
#define STRINGIFY(X) #X
#define MAKE_STRING(X) STRINGIFY(X)

const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";

// File name of the External ClearKey CDM on different platforms.
const base::FilePath::CharType kExternalClearKeyCdmFileName[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("libclearkeycdm.dylib");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("clearkeycdm.dll");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("libclearkeycdm.so");
#endif

// Random key ID used to create a session.
const uint8 kKeyId[] = {
    // base64 equivalent is AQIDBAUGBwgJCgsMDQ4PEA
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
};

const char kKeyIdAsJWK[] = "{\"kids\": [\"AQIDBAUGBwgJCgsMDQ4PEA\"]}";

const uint8 kKeyIdAsPssh[] = {
    0x00, 0x00, 0x00, 0x00, 'p',  's',  's',  'h',   // size = 0
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

  CdmAdapterTest() {}
  ~CdmAdapterTest() override {}

 protected:
  // Initializes the adapter. |expected_result| tests that the call succeeds
  // or generates an error.
  void InitializeAndExpect(base::FilePath library_path,
                           ExpectedResult expected_result) {
    CdmConfig cdm_config;  // default settings of false are sufficient.

    CdmAdapter::Create(
        kExternalClearKeyKeySystem, library_path, cdm_config,
        base::Bind(&CdmAdapterTest::OnSessionMessage, base::Unretained(this)),
        base::Bind(&CdmAdapterTest::OnSessionClosed, base::Unretained(this)),
        base::Bind(&CdmAdapterTest::OnLegacySessionError,
                   base::Unretained(this)),
        base::Bind(&CdmAdapterTest::OnSessionKeysChange,
                   base::Unretained(this)),
        base::Bind(&CdmAdapterTest::OnSessionExpirationUpdate,
                   base::Unretained(this)),
        base::Bind(&CdmAdapterTest::OnCdmCreated, base::Unretained(this),
                   expected_result));
    RunUntilIdle();
  }

  // Creates a new session using |key_id|. |session_id_| will be set
  // when the promise is resolved. |expected_result| tests that
  // CreateSessionAndGenerateRequest() succeeds or generates an error.
  void CreateSessionAndExpect(EmeInitDataType data_type,
                              const std::vector<uint8>& key_id,
                              ExpectedResult expected_result) {
    DCHECK(!key_id.empty());

    if (expected_result == SUCCESS) {
      EXPECT_CALL(*this,
                  OnSessionMessage(IsNotEmpty(), _, _, GURL::EmptyGURL()));
    }

    adapter_->CreateSessionAndGenerateRequest(
        MediaKeys::TEMPORARY_SESSION, data_type, key_id,
        CreateSessionPromise(expected_result));
    RunUntilIdle();
  }

  // Loads the session specified by |session_id|. |expected_result| tests
  // that LoadSession() succeeds or generates an error.
  void LoadSessionAndExpect(const std::string& session_id,
                            ExpectedResult expected_result) {
    DCHECK(!session_id.empty());
    ASSERT_EQ(expected_result, FAILURE) << "LoadSession not supported.";

    adapter_->LoadSession(MediaKeys::TEMPORARY_SESSION, session_id,
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
      EXPECT_CALL(*this,
                  OnSessionKeysChangeCalled(session_id, new_key_expected));
    } else {
      EXPECT_CALL(*this, OnSessionKeysChangeCalled(_, _)).Times(0);
    }

    adapter_->UpdateSession(session_id,
                            std::vector<uint8>(key.begin(), key.end()),
                            CreatePromise(expected_result));
    RunUntilIdle();
  }

  base::FilePath ExternalClearKeyLibrary() { return library_path_; }

  std::string SessionId() { return session_id_; }

 private:
  void SetUp() override {
    // Determine the location of the CDM. It is expected to be in the same
    // directory as the current module.
    base::FilePath current_module_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &current_module_dir));
    library_path_ =
        current_module_dir.Append(base::FilePath(kExternalClearKeyCdmFileName));
    ASSERT_TRUE(base::PathExists(library_path_)) << library_path_.value();

    // Now load the CDM library.
    base::NativeLibraryLoadError error;
    library_.Reset(base::LoadNativeLibrary(library_path_, &error));
    ASSERT_TRUE(library_.is_valid()) << error.ToString();

    // Call INITIALIZE_CDM_MODULE()
    typedef void (*InitializeCdmFunc)();
    InitializeCdmFunc initialize_cdm_func = reinterpret_cast<InitializeCdmFunc>(
        library_.GetFunctionPointer(MAKE_STRING(INITIALIZE_CDM_MODULE)));
    ASSERT_TRUE(initialize_cdm_func) << "No INITIALIZE_CDM_MODULE in library";
    initialize_cdm_func();
  }

  void TearDown() override {
    // Call DeinitializeCdmModule()
    typedef void (*DeinitializeCdmFunc)();
    DeinitializeCdmFunc deinitialize_cdm_func =
        reinterpret_cast<DeinitializeCdmFunc>(
            library_.GetFunctionPointer("DeinitializeCdmModule"));
    ASSERT_TRUE(deinitialize_cdm_func)
        << "No DeinitializeCdmModule() in library";
    deinitialize_cdm_func();
  }

  void OnCdmCreated(ExpectedResult expected_result,
                    const scoped_refptr<MediaKeys>& cdm,
                    const std::string& error_message) {
    if (cdm) {
      EXPECT_EQ(expected_result, SUCCESS) << "CDM should not have loaded.";
      adapter_ = cdm;
    } else {
      EXPECT_EQ(expected_result, FAILURE) << error_message;
    }
  }

  // Create a promise. |expected_result| is used to indicate how the promise
  // should be fulfilled.
  scoped_ptr<SimpleCdmPromise> CreatePromise(ExpectedResult expected_result) {
    if (expected_result == SUCCESS) {
      EXPECT_CALL(*this, OnResolve());
    } else {
      EXPECT_CALL(*this, OnReject(_, _, IsNotEmpty()));
    }

    scoped_ptr<SimpleCdmPromise> promise(new CdmCallbackPromise<>(
        base::Bind(&CdmAdapterTest::OnResolve, base::Unretained(this)),
        base::Bind(&CdmAdapterTest::OnReject, base::Unretained(this))));
    return promise.Pass();
  }

  // Create a promise to be used when a new session is created.
  // |expected_result| is used to indicate how the promise should be fulfilled.
  scoped_ptr<NewSessionCdmPromise> CreateSessionPromise(
      ExpectedResult expected_result) {
    if (expected_result == SUCCESS) {
      EXPECT_CALL(*this, OnResolveWithSession(_))
          .WillOnce(SaveArg<0>(&session_id_));
    } else {
      EXPECT_CALL(*this, OnReject(_, _, IsNotEmpty()));
    }

    scoped_ptr<NewSessionCdmPromise> promise(
        new CdmCallbackPromise<std::string>(
            base::Bind(&CdmAdapterTest::OnResolveWithSession,
                       base::Unretained(this)),
            base::Bind(&CdmAdapterTest::OnReject, base::Unretained(this))));
    return promise.Pass();
  }

  void RunUntilIdle() { message_loop_.RunUntilIdle(); }

  // Methods used for promise resolved/rejected.
  MOCK_METHOD0(OnResolve, void());
  MOCK_METHOD1(OnResolveWithSession, void(const std::string& session_id));
  MOCK_METHOD3(OnReject,
               void(MediaKeys::Exception exception_code,
                    uint32 system_code,
                    const std::string& error_message));

  // Methods used for the events possibly generated by CdmAdapater.
  MOCK_METHOD4(OnSessionMessage,
               void(const std::string& session_id,
                    MediaKeys::MessageType message_type,
                    const std::vector<uint8_t>& message,
                    const GURL& legacy_destination_url));
  MOCK_METHOD1(OnSessionClosed, void(const std::string& session_id));
  MOCK_METHOD4(OnLegacySessionError,
               void(const std::string& session_id,
                    MediaKeys::Exception exception,
                    uint32_t system_code,
                    const std::string& error_message));
  MOCK_METHOD2(OnSessionKeysChangeCalled,
               void(const std::string& session_id,
                    bool has_additional_usable_key));
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info) {
    // MOCK methods don't like CdmKeysInfo.
    OnSessionKeysChangeCalled(session_id, has_additional_usable_key);
  }
  MOCK_METHOD2(OnSessionExpirationUpdate,
               void(const std::string& session_id,
                    const base::Time& new_expiry_time));

  // Keep a reference to the CDM.
  base::FilePath library_path_;
  base::ScopedNativeLibrary library_;

  scoped_refptr<MediaKeys> adapter_;

  // |session_id_| is the latest result of calling CreateSession().
  std::string session_id_;

  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(CdmAdapterTest);
};

TEST_F(CdmAdapterTest, Initialize) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);
}

TEST_F(CdmAdapterTest, BadLibraryPath) {
  InitializeAndExpect(base::FilePath(FILE_PATH_LITERAL("no_library_here")),
                      FAILURE);
}

TEST_F(CdmAdapterTest, CreateWebmSession) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);

  std::vector<uint8> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);
}

TEST_F(CdmAdapterTest, CreateKeyIdsSession) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);

  // Don't include the trailing /0 from the string in the data passed in.
  std::vector<uint8> key_id(kKeyIdAsJWK,
                            kKeyIdAsJWK + arraysize(kKeyIdAsJWK) - 1);
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, SUCCESS);
}

TEST_F(CdmAdapterTest, CreateCencSession) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);

  std::vector<uint8> key_id(kKeyIdAsPssh,
                            kKeyIdAsPssh + arraysize(kKeyIdAsPssh));
#if defined(USE_PROPRIETARY_CODECS)
  CreateSessionAndExpect(EmeInitDataType::CENC, key_id, SUCCESS);
#else
  CreateSessionAndExpect(EmeInitDataType::CENC, key_id, FAILURE);
#endif
}

TEST_F(CdmAdapterTest, CreateSessionWithBadData) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);

  // Use |kKeyId| but specify KEYIDS format.
  std::vector<uint8> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, FAILURE);
}

TEST_F(CdmAdapterTest, LoadSession) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);

  // LoadSession() is not supported by AesDecryptor.
  std::vector<uint8> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, FAILURE);
}

TEST_F(CdmAdapterTest, UpdateSession) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);

  std::vector<uint8> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);

  UpdateSessionAndExpect(SessionId(), kKeyAsJWK, SUCCESS, true);
}

TEST_F(CdmAdapterTest, UpdateSessionWithBadData) {
  InitializeAndExpect(ExternalClearKeyLibrary(), SUCCESS);

  std::vector<uint8> key_id(kKeyId, kKeyId + arraysize(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);

  UpdateSessionAndExpect(SessionId(), "random data", FAILURE, true);
}

}  // namespace media
