// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/cdm/key_system_names.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "media/mojo/services/mojo_demuxer_stream_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Exactly;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::StrictMock;

namespace media {
namespace {

const char kInvalidKeySystem[] = "invalid.key.system";
const char kSecurityOrigin[] = "http://foo.com";

class MockMediaRendererClient : public mojo::MediaRendererClient {
 public:
  MockMediaRendererClient(){};
  ~MockMediaRendererClient() override{};

  // mojo::MediaRendererClient implementation.
  MOCK_METHOD2(OnTimeUpdate, void(int64_t time_usec, int64_t max_time_usec));
  MOCK_METHOD1(OnBufferingStateChange, void(mojo::BufferingState state));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD0(OnError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaRendererClient);
};

class MediaAppTest : public mojo::test::ApplicationTestBase {
 public:
  MediaAppTest()
      : media_renderer_client_binding_(&media_renderer_client_),
        video_demuxer_stream_(DemuxerStream::VIDEO) {}
  ~MediaAppTest() override {}

  void SetUp() override {
    ApplicationTestBase::SetUp();

    mojo::URLRequestPtr request = mojo::URLRequest::New();
    request->url = "mojo:media";
    mojo::ApplicationConnection* connection =
        application_impl()->ConnectToApplication(request.Pass());

    connection->ConnectToService(&cdm_);
    connection->ConnectToService(&media_renderer_);

    run_loop_.reset(new base::RunLoop());
  }

  // MOCK_METHOD* doesn't support move only types. Work around this by having
  // an extra method.
  MOCK_METHOD1(OnCdmInitializedInternal, void(bool result));
  void OnCdmInitialized(mojo::CdmPromiseResultPtr result) {
    OnCdmInitializedInternal(result->success);
  }

  void InitializeCdm(const std::string& key_system, bool expected_result) {
    EXPECT_CALL(*this, OnCdmInitializedInternal(expected_result))
        .Times(Exactly(1))
        .WillOnce(InvokeWithoutArgs(run_loop_.get(), &base::RunLoop::Quit));
    cdm_->Initialize(
        key_system, kSecurityOrigin, 1,
        base::Bind(&MediaAppTest::OnCdmInitialized, base::Unretained(this)));
  }

  MOCK_METHOD0(OnRendererInitialized, void());

  void InitializeRenderer(const VideoDecoderConfig& video_config) {
    video_demuxer_stream_.set_video_decoder_config(video_config);

    mojo::DemuxerStreamPtr video_stream;
    new MojoDemuxerStreamImpl(&video_demuxer_stream_, GetProxy(&video_stream));

    mojo::MediaRendererClientPtr client_ptr;
    media_renderer_client_binding_.Bind(GetProxy(&client_ptr));

    EXPECT_CALL(*this, OnRendererInitialized())
        .Times(Exactly(1))
        .WillOnce(Invoke(run_loop_.get(), &base::RunLoop::Quit));
    media_renderer_->Initialize(client_ptr.Pass(), nullptr, video_stream.Pass(),
                                base::Bind(&MediaAppTest::OnRendererInitialized,
                                           base::Unretained(this)));
  }

 protected:
  scoped_ptr<base::RunLoop> run_loop_;

  mojo::ContentDecryptionModulePtr cdm_;
  mojo::MediaRendererPtr media_renderer_;

  StrictMock<MockMediaRendererClient> media_renderer_client_;
  mojo::Binding<mojo::MediaRendererClient> media_renderer_client_binding_;

  StrictMock<MockDemuxerStream> video_demuxer_stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaAppTest);
};

}  // namespace

// Note: base::RunLoop::RunUntilIdle() does not work well in these tests because
// even when the loop is idle, we may still have pending events in the pipe.

TEST_F(MediaAppTest, InitializeCdm_Success) {
  InitializeCdm(kClearKey, true);
  run_loop_->Run();
}

TEST_F(MediaAppTest, InitializeCdm_InvalidKeySystem) {
  InitializeCdm(kInvalidKeySystem, false);
  run_loop_->Run();
}

TEST_F(MediaAppTest, InitializeRenderer_Success) {
  InitializeRenderer(TestVideoConfig::Normal());
  run_loop_->Run();
}

TEST_F(MediaAppTest, InitializeRenderer_InvalidConfig) {
  EXPECT_CALL(media_renderer_client_, OnError());
  InitializeRenderer(TestVideoConfig::Invalid());
  run_loop_->Run();
}

}  // namespace media
