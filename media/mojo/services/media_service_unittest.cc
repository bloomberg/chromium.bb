// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/base/cdm_config.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/mojo/clients/mojo_demuxer_stream_impl.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/features.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "media/mojo/interfaces/renderer.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::Exactly;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::StrictMock;

namespace media {
namespace {

#if BUILDFLAG(ENABLE_MOJO_CDM)
const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kInvalidKeySystem[] = "invalid.key.system";
#endif
const char kSecurityOrigin[] = "http://foo.com";

class MockRendererClient : public mojom::RendererClient {
 public:
  MockRendererClient() = default;
  ~MockRendererClient() override = default;

  // mojom::RendererClient implementation.
  MOCK_METHOD3(OnTimeUpdate,
               void(base::TimeDelta time,
                    base::TimeDelta max_time,
                    base::TimeTicks capture_time));
  MOCK_METHOD1(OnBufferingStateChange, void(BufferingState state));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnVideoOpacityChange, void(bool opaque));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size& size));
  MOCK_METHOD1(OnStatisticsUpdate,
               void(const media::PipelineStatistics& stats));
  MOCK_METHOD0(OnWaitingForDecryptionKey, void());
  MOCK_METHOD1(OnDurationChange, void(base::TimeDelta duration));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRendererClient);
};

class MediaServiceTest : public service_manager::test::ServiceTest {
 public:
  MediaServiceTest()
      : ServiceTest("media_service_unittests"),
        renderer_client_binding_(&renderer_client_),
        video_stream_(DemuxerStream::VIDEO) {}
  ~MediaServiceTest() override = default;

  void SetUp() override {
    ServiceTest::SetUp();

    media::mojom::MediaServicePtr media_service;
    connector()->BindInterface(media::mojom::kMediaServiceName, &media_service);

    service_manager::mojom::InterfaceProviderPtr interfaces;
    auto provider = std::make_unique<MediaInterfaceProvider>(
        mojo::MakeRequest(&interfaces));
    media_service->CreateInterfaceFactory(
        mojo::MakeRequest(&interface_factory_), std::move(interfaces));

    run_loop_.reset(new base::RunLoop());
  }

  // MOCK_METHOD* doesn't support move only types. Work around this by having
  // an extra method.
  MOCK_METHOD2(OnCdmInitializedInternal, void(bool result, int cdm_id));
  void OnCdmInitialized(mojom::CdmPromiseResultPtr result,
                        int cdm_id,
                        mojom::DecryptorPtr decryptor) {
    OnCdmInitializedInternal(result->success, cdm_id);
  }

  void InitializeCdm(const std::string& key_system,
                     bool expected_result,
                     int cdm_id) {
    interface_factory_->CreateCdm(key_system, mojo::MakeRequest(&cdm_));

    EXPECT_CALL(*this, OnCdmInitializedInternal(expected_result, cdm_id))
        .Times(Exactly(1))
        .WillOnce(InvokeWithoutArgs(run_loop_.get(), &base::RunLoop::Quit));
    cdm_->Initialize(key_system, url::Origin::Create(GURL(kSecurityOrigin)),
                     CdmConfig(),
                     base::Bind(&MediaServiceTest::OnCdmInitialized,
                                base::Unretained(this)));
  }

  MOCK_METHOD1(OnRendererInitialized, void(bool));

  void InitializeRenderer(const VideoDecoderConfig& video_config,
                          bool expected_result) {
    interface_factory_->CreateRenderer(std::string(),
                                       mojo::MakeRequest(&renderer_));

    video_stream_.set_video_decoder_config(video_config);

    mojom::DemuxerStreamPtrInfo video_stream_proxy_info;
    mojo_video_stream_.reset(new MojoDemuxerStreamImpl(
        &video_stream_, MakeRequest(&video_stream_proxy_info)));

    mojom::RendererClientAssociatedPtrInfo client_ptr_info;
    renderer_client_binding_.Bind(mojo::MakeRequest(&client_ptr_info));

    EXPECT_CALL(*this, OnRendererInitialized(expected_result))
        .Times(Exactly(1))
        .WillOnce(InvokeWithoutArgs(run_loop_.get(), &base::RunLoop::Quit));
    std::vector<mojom::DemuxerStreamPtrInfo> streams;
    streams.push_back(std::move(video_stream_proxy_info));
    renderer_->Initialize(std::move(client_ptr_info), std::move(streams),
                          base::nullopt, base::nullopt,
                          base::Bind(&MediaServiceTest::OnRendererInitialized,
                                     base::Unretained(this)));
  }

  MOCK_METHOD0(ConnectionClosed, void());

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;

  mojom::InterfaceFactoryPtr interface_factory_;
  mojom::ContentDecryptionModulePtr cdm_;
  mojom::RendererPtr renderer_;

  StrictMock<MockRendererClient> renderer_client_;
  mojo::AssociatedBinding<mojom::RendererClient> renderer_client_binding_;

  StrictMock<MockDemuxerStream> video_stream_;
  std::unique_ptr<MojoDemuxerStreamImpl> mojo_video_stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaServiceTest);
};

}  // namespace

// Note: base::RunLoop::RunUntilIdle() does not work well in these tests because
// even when the loop is idle, we may still have pending events in the pipe.

#if BUILDFLAG(ENABLE_MOJO_CDM)
TEST_F(MediaServiceTest, InitializeCdm_Success) {
  InitializeCdm(kClearKeyKeySystem, true, 1);
  run_loop_->Run();
}

TEST_F(MediaServiceTest, InitializeCdm_InvalidKeySystem) {
  InitializeCdm(kInvalidKeySystem, false, 0);
  run_loop_->Run();
}
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
// Sometimes fails on Linux. http://crbug.com/594977
#if defined(OS_LINUX)
#define MAYBE_InitializeRenderer_Success DISABLED_InitializeRenderer_Success
#else
#define MAYBE_InitializeRenderer_Success InitializeRenderer_Success
#endif

TEST_F(MediaServiceTest, MAYBE_InitializeRenderer_Success) {
  InitializeRenderer(TestVideoConfig::Normal(), true);
  run_loop_->Run();
}

TEST_F(MediaServiceTest, InitializeRenderer_InvalidConfig) {
  InitializeRenderer(TestVideoConfig::Invalid(), false);
  run_loop_->Run();
}
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

TEST_F(MediaServiceTest, Lifetime) {
  // The lifetime of the media service is controlled by the number of
  // live InterfaceFactory impls, not MediaService impls, so this pipe should
  // be closed when the last InterfaceFactory is destroyed.
  media::mojom::MediaServicePtr media_service;
  connector()->BindInterface(media::mojom::kMediaServiceName, &media_service);
  media_service.set_connection_error_handler(
      base::Bind(&MediaServiceTest::ConnectionClosed, base::Unretained(this)));

  // Disconnecting CDM and Renderer services doesn't terminate the app.
  cdm_.reset();
  renderer_.reset();

  // Disconnecting InterfaceFactory service should terminate the app, which will
  // close the connection.
  EXPECT_CALL(*this, ConnectionClosed())
      .Times(Exactly(1))
      .WillOnce(Invoke(run_loop_.get(), &base::RunLoop::Quit));
  interface_factory_.reset();

  run_loop_->Run();
}

}  // namespace media
