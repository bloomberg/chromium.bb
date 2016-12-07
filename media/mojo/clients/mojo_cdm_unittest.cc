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
#include "media/base/mock_filters.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/mojo/clients/mojo_cdm.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace media {

namespace {

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kTestSecurityOrigin[] = "https://www.test.com";

}  // namespace

class MojoCdmTest : public ::testing::Test {
 public:
  enum ExpectedResult { SUCCESS, CONNECTION_ERROR };

  MojoCdmTest()
      : mojo_cdm_service_(base::MakeUnique<MojoCdmService>(
            mojo_cdm_service_context_.GetWeakPtr(),
            &cdm_factory_)),
        cdm_binding_(mojo_cdm_service_.get()) {}

  virtual ~MojoCdmTest() {}

  void Initialize(ExpectedResult expected_result) {
    mojom::ContentDecryptionModulePtr remote_cdm;
    auto cdm_request = mojo::GetProxy(&remote_cdm);

    switch (expected_result) {
      case SUCCESS:
        cdm_binding_.Bind(std::move(cdm_request));
        break;
      case CONNECTION_ERROR:
        cdm_request.ResetWithReason(0, "Request dropped for testing.");
        break;
    }

    MojoCdm::Create(kClearKeyKeySystem, GURL(kTestSecurityOrigin), CdmConfig(),
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

    if (expected_result == SUCCESS) {
      EXPECT_TRUE(mojo_cdm_);
    } else {
      EXPECT_FALSE(mojo_cdm_);
    }
  }

  void OnCdmCreated(ExpectedResult expected_result,
                    const scoped_refptr<MediaKeys>& cdm,
                    const std::string& error_message) {
    if (!cdm) {
      DVLOG(1) << error_message;
      return;
    }

    EXPECT_EQ(SUCCESS, expected_result);
    mojo_cdm_ = cdm;
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
  scoped_refptr<MediaKeys> mojo_cdm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoCdmTest);
};

TEST_F(MojoCdmTest, Create_Success) {
  Initialize(SUCCESS);
}

TEST_F(MojoCdmTest, Create_ConnectionError) {
  Initialize(CONNECTION_ERROR);
}

// TODO(xhwang): Add more test cases!

}  // namespace media
