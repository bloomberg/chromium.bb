// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/cdm/key_system_names.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "media/mojo/interfaces/renderer.mojom.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/media_type_converters.h"
#include "media/mojo/services/mojo_demuxer_stream_impl.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Exactly;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::StrictMock;

namespace media {
namespace {

const char kInvalidKeySystem[] = "invalid.key.system";
const char kSecurityOrigin[] = "http://foo.com";

class MockRendererClient : public interfaces::RendererClient {
 public:
  MockRendererClient(){};
  ~MockRendererClient() override{};

  // interfaces::RendererClient implementation.
  MOCK_METHOD2(OnTimeUpdate, void(int64_t time_usec, int64_t max_time_usec));
  MOCK_METHOD1(OnBufferingStateChange, void(interfaces::BufferingState state));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD0(OnError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRendererClient);
};

class MediaAppTest : public mojo::test::ApplicationTestBase {
 public:
  MediaAppTest()
      : renderer_client_binding_(&renderer_client_),
        video_demuxer_stream_(DemuxerStream::VIDEO) {}
  ~MediaAppTest() override {}

  void SetUp() override {
    ApplicationTestBase::SetUp();

    connection_ = application_impl()->ConnectToApplication("mojo:media");
    connection_->SetRemoteServiceProviderConnectionErrorHandler(
        base::Bind(&MediaAppTest::ConnectionClosed, base::Unretained(this)));

    connection_->ConnectToService(&service_factory_);
    service_factory_->CreateCdm(mojo::GetProxy(&cdm_));
    service_factory_->CreateRenderer(mojo::GetProxy(&renderer_));

    run_loop_.reset(new base::RunLoop());
  }

  // MOCK_METHOD* doesn't support move only types. Work around this by having
  // an extra method.
  MOCK_METHOD2(OnCdmInitializedInternal, void(bool result, int cdm_id));
  void OnCdmInitialized(interfaces::CdmPromiseResultPtr result,
                        int cdm_id,
                        interfaces::DecryptorPtr decryptor) {
    OnCdmInitializedInternal(result->success, cdm_id);
  }

  void InitializeCdm(const std::string& key_system,
                     bool expected_result,
                     int cdm_id) {
    EXPECT_CALL(*this, OnCdmInitializedInternal(expected_result, cdm_id))
        .Times(Exactly(1))
        .WillOnce(InvokeWithoutArgs(run_loop_.get(), &base::RunLoop::Quit));
    cdm_->Initialize(
        key_system, kSecurityOrigin, interfaces::CdmConfig::From(CdmConfig()),
        base::Bind(&MediaAppTest::OnCdmInitialized, base::Unretained(this)));
  }

  MOCK_METHOD1(OnRendererInitialized, void(bool));

  void InitializeRenderer(const VideoDecoderConfig& video_config,
                          bool expected_result) {
    video_demuxer_stream_.set_video_decoder_config(video_config);

    interfaces::DemuxerStreamPtr video_stream;
    new MojoDemuxerStreamImpl(&video_demuxer_stream_, GetProxy(&video_stream));

    interfaces::RendererClientPtr client_ptr;
    renderer_client_binding_.Bind(GetProxy(&client_ptr));

    EXPECT_CALL(*this, OnRendererInitialized(expected_result))
        .Times(Exactly(1))
        .WillOnce(InvokeWithoutArgs(run_loop_.get(), &base::RunLoop::Quit));
    renderer_->Initialize(std::move(client_ptr), nullptr,
                          std::move(video_stream),
                          base::Bind(&MediaAppTest::OnRendererInitialized,
                                     base::Unretained(this)));
  }

  MOCK_METHOD0(ConnectionClosed, void());

 protected:
  scoped_ptr<base::RunLoop> run_loop_;

  interfaces::ServiceFactoryPtr service_factory_;
  interfaces::ContentDecryptionModulePtr cdm_;
  interfaces::RendererPtr renderer_;

  StrictMock<MockRendererClient> renderer_client_;
  mojo::Binding<interfaces::RendererClient> renderer_client_binding_;

  StrictMock<MockDemuxerStream> video_demuxer_stream_;

 private:
  scoped_ptr<mojo::ApplicationConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(MediaAppTest);
};

}  // namespace

// Note: base::RunLoop::RunUntilIdle() does not work well in these tests because
// even when the loop is idle, we may still have pending events in the pipe.

TEST_F(MediaAppTest, InitializeCdm_Success) {
  InitializeCdm(kClearKey, true, 1);
  run_loop_->Run();
}

TEST_F(MediaAppTest, InitializeCdm_InvalidKeySystem) {
  InitializeCdm(kInvalidKeySystem, false, 0);
  run_loop_->Run();
}

TEST_F(MediaAppTest, InitializeRenderer_Success) {
  InitializeRenderer(TestVideoConfig::Normal(), true);
  run_loop_->Run();
}

TEST_F(MediaAppTest, InitializeRenderer_InvalidConfig) {
  InitializeRenderer(TestVideoConfig::Invalid(), false);
  run_loop_->Run();
}

TEST_F(MediaAppTest, Lifetime) {
  // Disconnecting CDM and Renderer services doesn't terminate the app.
  cdm_.reset();
  renderer_.reset();

  // Disconnecting ServiceFactory service should terminate the app, which will
  // close the connection.
  EXPECT_CALL(*this, ConnectionClosed())
      .Times(Exactly(1))
      .WillOnce(Invoke(run_loop_.get(), &base::RunLoop::Quit));
  service_factory_.reset();

  run_loop_->Run();
}

}  // namespace media
