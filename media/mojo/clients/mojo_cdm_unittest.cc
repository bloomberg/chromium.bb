// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/content_decryption_module.h"
#include "media/base/mock_filters.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/mojo/clients/mojo_cdm.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

MATCHER(NotEmpty, "") {
  return !arg.empty();
}

namespace media {

namespace {

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kTestSecurityOrigin[] = "https://www.test.com";

// Random key ID used to create a session.
const uint8_t kKeyId[] = {
    // base64 equivalent is AQIDBAUGBwgJCgsMDQ4PEA
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
};

}  // namespace

class MojoCdmTest : public ::testing::Test {
 public:
  enum ExpectedResult { SUCCESS, CONNECTION_ERROR, FAILURE };

  MojoCdmTest()
      : mojo_cdm_service_(base::MakeUnique<MojoCdmService>(
            mojo_cdm_service_context_.GetWeakPtr(),
            &cdm_factory_)),
        cdm_binding_(mojo_cdm_service_.get()) {}

  virtual ~MojoCdmTest() {}

  void Initialize(const std::string& key_system,
                  ExpectedResult expected_result) {
    mojom::ContentDecryptionModulePtr remote_cdm;
    auto cdm_request = mojo::MakeRequest(&remote_cdm);

    switch (expected_result) {
      case SUCCESS:
      case FAILURE:
        cdm_binding_.Bind(std::move(cdm_request));
        break;
      case CONNECTION_ERROR:
        cdm_request.ResetWithReason(0, "Request dropped for testing.");
        break;
    }

    MojoCdm::Create(key_system, GURL(kTestSecurityOrigin), CdmConfig(),
                    std::move(remote_cdm),
                    base::Bind(&MockCdmClient::OnSessionMessage,
                               base::Unretained(&cdm_client_)),
                    base::Bind(&MockCdmClient::OnSessionClosed,
                               base::Unretained(&cdm_client_)),
                    base::Bind(&MockCdmClient::OnSessionKeysChange,
                               base::Unretained(&cdm_client_)),
                    base::Bind(&MockCdmClient::OnSessionExpirationUpdate,
                               base::Unretained(&cdm_client_)),
                    base::Bind(&MojoCdmTest::OnCdmCreated,
                               base::Unretained(this), expected_result));

    base::RunLoop().RunUntilIdle();
  }

  void OnCdmCreated(ExpectedResult expected_result,
                    const scoped_refptr<ContentDecryptionModule>& cdm,
                    const std::string& error_message) {
    if (!cdm) {
      EXPECT_NE(SUCCESS, expected_result);
      DVLOG(1) << error_message;
      return;
    }

    EXPECT_EQ(SUCCESS, expected_result);
    mojo_cdm_ = cdm;
  }

  void ForceConnectionError() {
    // If there is an existing session it will get closed when the connection
    // is broken.
    if (!session_id_.empty()) {
      EXPECT_CALL(cdm_client_, OnSessionClosed(session_id_));
    }

    cdm_binding_.CloseWithReason(2, "Test closed connection.");

    base::RunLoop().RunUntilIdle();
  }

  void SetServerCertificateAndExpect(const std::vector<uint8_t>& certificate,
                                     ExpectedResult expected_result) {
    mojo_cdm_->SetServerCertificate(
        certificate,
        base::MakeUnique<MockCdmPromise>(expected_result == SUCCESS));

    base::RunLoop().RunUntilIdle();
  }

  void CreateSessionAndExpect(EmeInitDataType data_type,
                              const std::vector<uint8_t>& key_id,
                              ExpectedResult expected_result) {
    if (expected_result == SUCCESS) {
      EXPECT_CALL(cdm_client_, OnSessionMessage(NotEmpty(), _, _));
    }

    mojo_cdm_->CreateSessionAndGenerateRequest(
        ContentDecryptionModule::SessionType::TEMPORARY_SESSION, data_type,
        key_id, base::MakeUnique<MockCdmSessionPromise>(
                    expected_result == SUCCESS, &session_id_));

    base::RunLoop().RunUntilIdle();
  }

  void CloseSessionAndExpect(ExpectedResult expected_result) {
    DCHECK(!session_id_.empty()) << "CloseSessionAndExpect() must be called "
                                    "after a successful "
                                    "CreateSessionAndExpect()";

    if (expected_result == SUCCESS) {
      EXPECT_CALL(cdm_client_, OnSessionClosed(session_id_));
    }

    mojo_cdm_->CloseSession(session_id_, base::MakeUnique<MockCdmPromise>(
                                             expected_result == SUCCESS));

    base::RunLoop().RunUntilIdle();
  }

  // Fixture members.
  base::TestMessageLoop message_loop_;

  MojoCdmServiceContext mojo_cdm_service_context_;
  StrictMock<MockCdmClient> cdm_client_;

  // TODO(jrummell): Use a MockCdmFactory to create a MockCdm here for more test
  // coverage.
  DefaultCdmFactory cdm_factory_;

  std::unique_ptr<MojoCdmService> mojo_cdm_service_;
  mojo::Binding<mojom::ContentDecryptionModule> cdm_binding_;
  scoped_refptr<ContentDecryptionModule> mojo_cdm_;

  // |session_id_| is the latest successful result of calling CreateSession().
  std::string session_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoCdmTest);
};

TEST_F(MojoCdmTest, Create_Success) {
  Initialize(kClearKeyKeySystem, SUCCESS);
}

TEST_F(MojoCdmTest, Create_ConnectionError) {
  Initialize(kClearKeyKeySystem, CONNECTION_ERROR);
}

TEST_F(MojoCdmTest, Create_Failure) {
  // This fails as DefaultCdmFactory only supports Clear Key.
  Initialize("org.random.cdm", FAILURE);
}

TEST_F(MojoCdmTest, SetServerCertificate_AfterConnectionError) {
  Initialize(kClearKeyKeySystem, SUCCESS);
  ForceConnectionError();
  SetServerCertificateAndExpect({0, 1, 2}, FAILURE);
}

TEST_F(MojoCdmTest, CreateSessionAndGenerateRequest_AfterConnectionError) {
  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));

  Initialize(kClearKeyKeySystem, SUCCESS);
  ForceConnectionError();
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, FAILURE);
}

TEST_F(MojoCdmTest, CloseSession_Success) {
  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));

  Initialize(kClearKeyKeySystem, SUCCESS);
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);
  CloseSessionAndExpect(SUCCESS);
}

TEST_F(MojoCdmTest, CloseSession_AfterConnectionError) {
  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));

  Initialize(kClearKeyKeySystem, SUCCESS);
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);
  ForceConnectionError();
  CloseSessionAndExpect(FAILURE);
}

}  // namespace media
