// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "chromecast/common/mojom/constants.mojom.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "chromecast/media/cma/test/mock_cma_backend.h"
#include "chromecast/media/cma/test/mock_cma_backend_factory.h"
#include "chromecast/media/cma/test/mock_multiroom_manager.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace {

std::unique_ptr<service_manager::Connector> CreateConnector() {
  service_manager::mojom::ConnectorRequest request;
  return service_manager::Connector::Create(&request);
}
}  // namespace

namespace chromecast {
namespace media {
namespace {

const ::media::AudioParameters kDefaultAudioParams(
    ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
    ::media::CHANNEL_LAYOUT_STEREO,
    ::media::AudioParameters::kAudioCDSampleRate,
    256);

class CastAudioManagerTest : public testing::Test {
 public:
  CastAudioManagerTest()
      : media_thread_("CastMediaThread"), connector_(CreateConnector()) {
    CHECK(media_thread_.Start());

    // Set the test connector to override interface bindings.
    service_manager::Connector::TestApi connector_test_api(connector_.get());
    connector_test_api.OverrideBinderForTesting(
        service_manager::Identity(chromecast::mojom::kChromecastServiceName),
        mojom::MultiroomManager::Name_,
        base::BindRepeating(&CastAudioManagerTest::BindMultiroomManager,
                            base::Unretained(this)));

    backend_factory_ = std::make_unique<MockCmaBackendFactory>();
    audio_manager_ = std::make_unique<CastAudioManager>(
        std::make_unique<::media::TestAudioThread>(), &audio_log_factory_,
        base::BindRepeating(&CastAudioManagerTest::GetCmaBackendFactory,
                            base::Unretained(this)),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        media_thread_.task_runner(), connector_.get(), false);
  }

  ~CastAudioManagerTest() override { audio_manager_->Shutdown(); }
  CmaBackendFactory* GetCmaBackendFactory() { return backend_factory_.get(); }

  // Binds |multiroom_manager_| to the interface requested through the test
  // connector.
  void BindMultiroomManager(mojo::ScopedMessagePipeHandle handle) {
    multiroom_manager_.Bind(std::move(handle));
  }

 protected:
  bool OpenStream(::media::AudioOutputStream* stream) {
    bool success = stream->Open();
    // TODO(awolter) Determine a better way to ensure that all the tasks are
    // flushed.
    media_thread_.FlushForTesting();
    scoped_task_environment_.RunUntilIdle();
    media_thread_.FlushForTesting();
    media_thread_.FlushForTesting();
    return success;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::Thread media_thread_;
  ::media::FakeAudioLogFactory audio_log_factory_;
  std::unique_ptr<CastAudioManager> audio_manager_;
  std::unique_ptr<MockCmaBackendFactory> backend_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  MockMultiroomManager multiroom_manager_;
};

TEST_F(CastAudioManagerTest, MakeAudioOutputStreamProxy) {
  StrictMock<MockCmaBackend::AudioDecoder> audio_decoder;
  EXPECT_CALL(audio_decoder, SetDelegate(_)).Times(1);
  EXPECT_CALL(audio_decoder, SetConfig(_)).WillOnce(Return(true));

  auto backend = std::make_unique<StrictMock<MockCmaBackend>>();
  EXPECT_CALL(*backend, CreateAudioDecoder()).WillOnce(Return(&audio_decoder));
  EXPECT_CALL(*backend, Initialize()).WillOnce(Return(true));

  EXPECT_CALL(*backend_factory_, CreateBackend(_))
      .WillOnce(Invoke([&backend](const MediaPipelineDeviceParams&) {
        return std::move(backend);
      }));
  EXPECT_EQ(backend_factory_.get(), audio_manager_->backend_factory());

  ::media::AudioOutputStream* stream =
      audio_manager_->MakeAudioOutputStreamProxy(kDefaultAudioParams,
                                                 std::string());
  EXPECT_TRUE(OpenStream(stream));
  stream->Close();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
