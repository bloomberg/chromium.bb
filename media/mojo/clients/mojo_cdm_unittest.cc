// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "base/time/time.h"
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
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::ReturnNull;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::WithArgs;
using ::testing::WithoutArgs;

MATCHER(NotEmpty, "") {
  return !arg.empty();
}

ACTION_P2(CdmCreated, cdm, error_message) {
  arg0.Run(cdm, error_message);
}

ACTION_P3(InvokeFunction, classPointer, memberFunc, p1) {
  (classPointer->*memberFunc)(arg0, p1);
}

ACTION_P4(InvokeFunction2, classPointer, memberFunc, p1, p2) {
  (classPointer->*memberFunc)(arg0, p1, p2);
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
  enum ExpectedResult {
    SUCCESS,
    FAILURE,                  // Operation fails immediately.
    PENDING,                  // Operation never completes.
    CONNECTION_ERROR_BEFORE,  // Connection error happened before operation.
    CONNECTION_ERROR_DURING,  // Connection error happens during operation.
  };

  MojoCdmTest()
      : mojo_cdm_service_(
            std::make_unique<MojoCdmService>(&cdm_factory_,
                                             &mojo_cdm_service_context_)),
        cdm_binding_(mojo_cdm_service_.get()) {}

  virtual ~MojoCdmTest() = default;

  void Initialize(ExpectedResult expected_result) {
    // TODO(xhwang): Add pending init support.
    DCHECK_NE(PENDING, expected_result);

    mojom::ContentDecryptionModulePtr remote_cdm;
    auto cdm_request = mojo::MakeRequest(&remote_cdm);

    cdm_binding_.Bind(std::move(cdm_request));

    std::string key_system;
    if (expected_result == CONNECTION_ERROR_BEFORE) {
      // Break the connection before the call.
      ForceConnectionError();
    } else if (expected_result != FAILURE) {
      // In the remaining cases, CDM is expected, so provide a key system.
      key_system = kClearKeyKeySystem;

      if (expected_result == CONNECTION_ERROR_DURING) {
        // Create() will be successful, so provide a callback that will break
        // the connection before returning the CDM.
        cdm_factory_.SetBeforeCreationCB(base::Bind(
            &MojoCdmTest::ForceConnectionError, base::Unretained(this)));
      }
    }

    MojoCdm::Create(key_system, url::Origin::Create(GURL(kTestSecurityOrigin)),
                    CdmConfig(), std::move(remote_cdm),
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
    remote_cdm_ = cdm_factory_.GetCreatedCdm();
    EXPECT_EQ(kClearKeyKeySystem, remote_cdm_->GetKeySystem());
    EXPECT_EQ(kTestSecurityOrigin,
              remote_cdm_->GetSecurityOrigin().Serialize());
  }

  void ForceConnectionError() {
    cdm_binding_.CloseWithReason(2, "Test closed connection.");
    base::RunLoop().RunUntilIdle();
  }

  void SetServerCertificateAndExpect(const std::vector<uint8_t>& certificate,
                                     ExpectedResult expected_result) {
    if (expected_result == CONNECTION_ERROR_BEFORE) {
      // Break the connection before the call, so SetServerCertificate() is
      // never called.
      ForceConnectionError();
    } else {
      EXPECT_CALL(*remote_cdm_, OnSetServerCertificate(certificate, _))
          .WillOnce(WithArg<1>(InvokeFunction(this, &MojoCdmTest::HandlePromise,
                                              expected_result)));
    }

    mojo_cdm_->SetServerCertificate(
        certificate,
        std::make_unique<MockCdmPromise>(expected_result == SUCCESS));
    base::RunLoop().RunUntilIdle();
  }

  void CreateSessionAndExpect(const std::string& session_id,
                              ExpectedResult expected_result) {
    // Specify parameters needed to call CreateSessionAndGenerateRequest() in
    // order to verify that the data is passed properly.
    const CdmSessionType session_type = CdmSessionType::TEMPORARY_SESSION;
    const EmeInitDataType data_type = EmeInitDataType::WEBM;
    const std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));
    std::string created_session_id;

    if (expected_result == CONNECTION_ERROR_BEFORE) {
      // Break the connection before the call, so
      // CreateSessionAndGenerateRequest() is never called.
      ForceConnectionError();
    } else {
      EXPECT_CALL(*remote_cdm_, OnCreateSessionAndGenerateRequest(
                                    session_type, data_type, key_id, _))
          .WillOnce(WithArg<3>(
              InvokeFunction2(this, &MojoCdmTest::HandleSessionPromise,
                              session_id, expected_result)));
    }

    // Note that although it's called CreateSessionAndGenerateRequest, no
    // request is generated.
    mojo_cdm_->CreateSessionAndGenerateRequest(
        session_type, data_type, key_id,
        std::make_unique<MockCdmSessionPromise>(expected_result == SUCCESS,
                                                &created_session_id));
    base::RunLoop().RunUntilIdle();

    // If the session was "created" ...
    if (expected_result == SUCCESS) {
      // Returned session ID must match the session ID provided.
      EXPECT_EQ(session_id, created_session_id);
      base::RunLoop().RunUntilIdle();
    }
  }

  void LoadSessionAndExpect(const std::string& session_id,
                            ExpectedResult expected_result) {
    const CdmSessionType session_type =
        CdmSessionType::PERSISTENT_LICENSE_SESSION;
    std::string loaded_session_id;

    if (expected_result == CONNECTION_ERROR_BEFORE) {
      // Break the connection before the call, so LoadSession() is never called.
      ForceConnectionError();
    } else {
      EXPECT_CALL(*remote_cdm_, OnLoadSession(session_type, session_id, _))
          .WillOnce(WithArg<2>(
              InvokeFunction2(this, &MojoCdmTest::HandleSessionPromise,
                              session_id, expected_result)));
    }

    mojo_cdm_->LoadSession(session_type, session_id,
                           std::make_unique<MockCdmSessionPromise>(
                               expected_result == SUCCESS, &loaded_session_id));
    base::RunLoop().RunUntilIdle();

    // If the session was "loaded" ...
    if (expected_result == SUCCESS) {
      // Returned session ID must match the session ID provided.
      EXPECT_EQ(session_id, loaded_session_id);

      // MojoCdm expects the session to be closed, so invoke SessionClosedCB
      // to "close" it.
      EXPECT_CALL(cdm_client_, OnSessionClosed(session_id));
      remote_cdm_->CallSessionClosedCB(session_id);
      base::RunLoop().RunUntilIdle();
    }
  }

  void UpdateSessionAndExpect(const std::string& session_id,
                              ExpectedResult expected_result) {
    // Specify parameters to UpdateSession() in order to verify that
    // the data is passed properly.
    const std::vector<uint8_t> response = {1, 2, 3, 4, 5, 6};

    if (expected_result == CONNECTION_ERROR_BEFORE) {
      // Break the connection before the call, so UpdateSession() is never
      // called.
      ForceConnectionError();
    } else {
      EXPECT_CALL(*remote_cdm_, OnUpdateSession(session_id, response, _))
          .WillOnce(WithArg<2>(InvokeFunction(this, &MojoCdmTest::HandlePromise,
                                              expected_result)));
    }

    mojo_cdm_->UpdateSession(
        session_id, response,
        std::make_unique<MockCdmPromise>(expected_result == SUCCESS));
    base::RunLoop().RunUntilIdle();
  }

  void CloseSessionAndExpect(const std::string& session_id,
                             ExpectedResult expected_result) {
    if (expected_result == CONNECTION_ERROR_BEFORE) {
      // Break the connection before the call, so CloseSession() is never
      // called.
      ForceConnectionError();
    } else {
      EXPECT_CALL(*remote_cdm_, OnCloseSession(session_id, _))
          .WillOnce(WithArg<1>(InvokeFunction(this, &MojoCdmTest::HandlePromise,
                                              expected_result)));
    }

    mojo_cdm_->CloseSession(session_id, std::make_unique<MockCdmPromise>(
                                            expected_result == SUCCESS));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveSessionAndExpect(const std::string& session_id,
                              ExpectedResult expected_result) {
    if (expected_result == CONNECTION_ERROR_BEFORE) {
      // Break the connection before the call, so RemoveSession() is never
      // called.
      ForceConnectionError();
    } else {
      EXPECT_CALL(*remote_cdm_, OnRemoveSession(session_id, _))
          .WillOnce(WithArg<1>(InvokeFunction(this, &MojoCdmTest::HandlePromise,
                                              expected_result)));
    }

    mojo_cdm_->RemoveSession(session_id, std::make_unique<MockCdmPromise>(
                                             expected_result == SUCCESS));
    base::RunLoop().RunUntilIdle();
  }

  void HandlePromise(std::unique_ptr<SimpleCdmPromise>& promise,
                     ExpectedResult expected_result) {
    switch (expected_result) {
      case SUCCESS:
        promise->resolve();
        break;

      case FAILURE:
        promise->reject(media::CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                        "Promise rejected");
        break;

      case PENDING:
        // Store the promise and never fulfill it.
        pending_simple_cdm_promise_ = std::move(promise);
        break;

      case CONNECTION_ERROR_BEFORE:
        // Connection should be broken before this is called.
        NOTREACHED();
        break;

      case CONNECTION_ERROR_DURING:
        ForceConnectionError();

        // Now that the connection is broken the promise result won't be passed
        // back. However, since we check that every promise is fulfilled, we
        // need to do something with this promise. Resolve the promise to
        // fulfill it, but note that the original caller will get back a
        // failed promise due to the connection being broken.
        promise->resolve();
        break;
    }
  }

  void HandleSessionPromise(std::unique_ptr<NewSessionCdmPromise>& promise,
                            const std::string& session_id,
                            ExpectedResult expected_result) {
    switch (expected_result) {
      case SUCCESS:
        promise->resolve(session_id);
        break;

      case FAILURE:
        promise->reject(media::CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                        "Promise rejected");
        break;

      case PENDING:
        // Store the promise and never fulfill it.
        pending_new_session_cdm_promise_ = std::move(promise);
        break;

      case CONNECTION_ERROR_BEFORE:
        // Connection should be broken before this is called.
        NOTREACHED();
        break;

      case CONNECTION_ERROR_DURING:
        ForceConnectionError();

        // Now that the connection is broken the promise result won't be passed
        // back. However, since we check that every promise is fulfilled, we
        // need to do something with this promise. Resolve the promise to
        // fulfill it, but note that the original caller will get back a
        // failed promise due to the connection being broken.
        promise->resolve(session_id);
        break;
    }
  }

  // Fixture members.
  base::TestMessageLoop message_loop_;

  // |remote_cdm_| represents the CDM at the end of the mojo message pipe.
  MockCdm* remote_cdm_;
  MockCdmFactory cdm_factory_;

  MojoCdmServiceContext mojo_cdm_service_context_;
  StrictMock<MockCdmClient> cdm_client_;

  // Declared before |mojo_cdm_| so they are destroyed after it.
  std::unique_ptr<SimpleCdmPromise> pending_simple_cdm_promise_;
  std::unique_ptr<NewSessionCdmPromise> pending_new_session_cdm_promise_;

  std::unique_ptr<MojoCdmService> mojo_cdm_service_;
  mojo::Binding<mojom::ContentDecryptionModule> cdm_binding_;
  scoped_refptr<ContentDecryptionModule> mojo_cdm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoCdmTest);
};

TEST_F(MojoCdmTest, Create_Success) {
  Initialize(SUCCESS);
}

TEST_F(MojoCdmTest, Create_Failure) {
  Initialize(FAILURE);
}

TEST_F(MojoCdmTest, Create_ConnectionErrorBefore) {
  Initialize(CONNECTION_ERROR_BEFORE);
}

TEST_F(MojoCdmTest, Create_ConnectionErrorDuring) {
  Initialize(CONNECTION_ERROR_DURING);
}

TEST_F(MojoCdmTest, SetServerCertificate_Success) {
  const std::vector<uint8_t> certificate = {0, 1, 2};
  Initialize(SUCCESS);
  SetServerCertificateAndExpect(certificate, SUCCESS);
}

TEST_F(MojoCdmTest, SetServerCertificate_Failure) {
  const std::vector<uint8_t> certificate = {1, 2, 3, 4, 5};
  Initialize(SUCCESS);
  SetServerCertificateAndExpect(certificate, FAILURE);
}

TEST_F(MojoCdmTest, SetServerCertificate_Pending) {
  const std::vector<uint8_t> certificate = {1, 2, 3, 4, 5};
  Initialize(SUCCESS);
  SetServerCertificateAndExpect(certificate, PENDING);
}

TEST_F(MojoCdmTest, SetServerCertificate_ConnectionErrorBefore) {
  const std::vector<uint8_t> certificate = {3, 4};
  Initialize(SUCCESS);
  SetServerCertificateAndExpect(certificate, CONNECTION_ERROR_BEFORE);
}

TEST_F(MojoCdmTest, SetServerCertificate_ConnectionErrorDuring) {
  const std::vector<uint8_t> certificate = {10, 11, 12};
  Initialize(SUCCESS);
  SetServerCertificateAndExpect(certificate, CONNECTION_ERROR_DURING);
}

TEST_F(MojoCdmTest, CreateSession_Success) {
  const std::string session_id = "create1";
  Initialize(SUCCESS);
  CreateSessionAndExpect(session_id, SUCCESS);

  // Created session should always be closed!
  EXPECT_CALL(cdm_client_, OnSessionClosed(session_id));
}

TEST_F(MojoCdmTest, CreateSession_Failure) {
  const std::string session_id = "create2";
  Initialize(SUCCESS);
  CreateSessionAndExpect(session_id, FAILURE);
}

TEST_F(MojoCdmTest, CreateSession_Pending) {
  const std::string session_id = "create2";
  Initialize(SUCCESS);
  CreateSessionAndExpect(session_id, PENDING);
}

TEST_F(MojoCdmTest, CreateSession_ConnectionErrorBefore) {
  const std::string session_id = "create3";
  Initialize(SUCCESS);
  CreateSessionAndExpect(session_id, CONNECTION_ERROR_BEFORE);
}

TEST_F(MojoCdmTest, CreateSession_ConnectionErrorDuring) {
  const std::string session_id = "create4";
  Initialize(SUCCESS);
  CreateSessionAndExpect(session_id, CONNECTION_ERROR_DURING);
}

TEST_F(MojoCdmTest, LoadSession_Success) {
  const std::string session_id = "load1";
  Initialize(SUCCESS);
  LoadSessionAndExpect(session_id, SUCCESS);
}

TEST_F(MojoCdmTest, LoadSession_Failure) {
  const std::string session_id = "load2";
  Initialize(SUCCESS);
  LoadSessionAndExpect(session_id, FAILURE);
}

TEST_F(MojoCdmTest, LoadSession_Pending) {
  const std::string session_id = "load2";
  Initialize(SUCCESS);
  LoadSessionAndExpect(session_id, PENDING);
}

TEST_F(MojoCdmTest, LoadSession_ConnectionErrorBefore) {
  const std::string session_id = "load3";
  Initialize(SUCCESS);
  LoadSessionAndExpect(session_id, CONNECTION_ERROR_BEFORE);
}

TEST_F(MojoCdmTest, LoadSession_ConnectionErrorDuring) {
  const std::string session_id = "load4";
  Initialize(SUCCESS);
  LoadSessionAndExpect(session_id, CONNECTION_ERROR_DURING);
}

TEST_F(MojoCdmTest, UpdateSession_Success) {
  const std::string session_id = "update1";
  Initialize(SUCCESS);
  UpdateSessionAndExpect(session_id, SUCCESS);
}

TEST_F(MojoCdmTest, UpdateSession_Failure) {
  const std::string session_id = "update2";
  Initialize(SUCCESS);
  UpdateSessionAndExpect(session_id, FAILURE);
}

TEST_F(MojoCdmTest, UpdateSession_Pending) {
  const std::string session_id = "update2";
  Initialize(SUCCESS);
  UpdateSessionAndExpect(session_id, PENDING);
}

TEST_F(MojoCdmTest, UpdateSession_ConnectionErrorBefore) {
  const std::string session_id = "update3";
  Initialize(SUCCESS);
  UpdateSessionAndExpect(session_id, CONNECTION_ERROR_BEFORE);
}

TEST_F(MojoCdmTest, UpdateSession_ConnectionErrorDuring) {
  const std::string session_id = "update4";
  Initialize(SUCCESS);
  UpdateSessionAndExpect(session_id, CONNECTION_ERROR_DURING);
}

TEST_F(MojoCdmTest, CloseSession_Success) {
  const std::string session_id = "close1";
  Initialize(SUCCESS);
  CloseSessionAndExpect(session_id, SUCCESS);
}

TEST_F(MojoCdmTest, CloseSession_Failure) {
  const std::string session_id = "close2";
  Initialize(SUCCESS);
  CloseSessionAndExpect(session_id, FAILURE);
}

TEST_F(MojoCdmTest, CloseSession_Pending) {
  const std::string session_id = "close2";
  Initialize(SUCCESS);
  CloseSessionAndExpect(session_id, PENDING);
}

TEST_F(MojoCdmTest, CloseSession_ConnectionErrorBefore) {
  const std::string session_id = "close3";
  Initialize(SUCCESS);
  CloseSessionAndExpect(session_id, CONNECTION_ERROR_BEFORE);
}

TEST_F(MojoCdmTest, CloseSession_ConnectionErrorDuring) {
  const std::string session_id = "close4";
  Initialize(SUCCESS);
  CloseSessionAndExpect(session_id, CONNECTION_ERROR_DURING);
}

TEST_F(MojoCdmTest, RemoveSession_Success) {
  const std::string session_id = "remove1";
  Initialize(SUCCESS);
  RemoveSessionAndExpect(session_id, SUCCESS);
}

TEST_F(MojoCdmTest, RemoveSession_Failure) {
  const std::string session_id = "remove2";
  Initialize(SUCCESS);
  RemoveSessionAndExpect(session_id, FAILURE);
}

TEST_F(MojoCdmTest, RemoveSession_Pending) {
  const std::string session_id = "remove2";
  Initialize(SUCCESS);
  RemoveSessionAndExpect(session_id, PENDING);
}

TEST_F(MojoCdmTest, RemoveSession_ConnectionErrorBefore) {
  const std::string session_id = "remove3";
  Initialize(SUCCESS);
  RemoveSessionAndExpect(session_id, CONNECTION_ERROR_BEFORE);
}

TEST_F(MojoCdmTest, RemoveSession_ConnectionErrorDuring) {
  const std::string session_id = "remove4";
  Initialize(SUCCESS);
  RemoveSessionAndExpect(session_id, CONNECTION_ERROR_DURING);
}

// Note that MojoCdm requires a session to exist when SessionClosedCB is called,
// so it is currently tested in the success cases for CreateSession/LoadSession.

TEST_F(MojoCdmTest, SessionMessageCB_Success) {
  const std::string session_id = "message";
  const CdmMessageType message_type = CdmMessageType::LICENSE_REQUEST;
  const std::vector<uint8_t> message = {0, 1, 2};
  Initialize(SUCCESS);
  EXPECT_CALL(cdm_client_, OnSessionMessage(session_id, message_type, message));
  remote_cdm_->CallSessionMessageCB(session_id, message_type, message);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmTest, SessionExpirationChangeCB_Success) {
  const std::string session_id = "expiration";
  const base::Time time = base::Time::Now();
  Initialize(SUCCESS);
  EXPECT_CALL(cdm_client_, OnSessionExpirationUpdate(session_id, time));
  remote_cdm_->CallSessionExpirationUpdateCB(session_id, time);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmTest, SessionKeysChangeCB_Success) {
  const std::string session_id = "change";
  bool has_additional_usable_key = true;
  CdmKeysInfo keys_info;
  Initialize(SUCCESS);
  EXPECT_CALL(cdm_client_,
              OnSessionKeysChangeCalled(session_id, has_additional_usable_key));
  remote_cdm_->CallSessionKeysChangeCB(session_id, has_additional_usable_key,
                                       std::move(keys_info));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
