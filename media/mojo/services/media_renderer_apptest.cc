// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/sample_format.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/services/mojo_renderer_impl.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_test_base.h"
#include "mojo/public/cpp/application/connect.h"

namespace {

// This class is here to give the gtest class access to the
// mojo::ApplicationImpl so that the tests can connect to other applications.
class MojoRendererTestHelper : public mojo::ApplicationDelegate {
 public:
  MojoRendererTestHelper() : application_impl_(NULL) {}
  ~MojoRendererTestHelper() override {}

  // ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) override {
    application_impl_ = app;
  }

  mojo::ApplicationImpl* application_impl() { return application_impl_; }

 private:
  mojo::ApplicationImpl* application_impl_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererTestHelper);
};

// TODO(tim): Make media::FakeDemuxerStream support audio and use that for the
// DemuxerStream implementation instead.
class FakeDemuxerStream : public media::DemuxerStreamProvider,
                          public media::DemuxerStream {
 public:
  FakeDemuxerStream() {}
  ~FakeDemuxerStream() override {}

  // media::Demuxer implementation.
  media::DemuxerStream* GetStream(media::DemuxerStream::Type type) override {
    if (type == media::DemuxerStream::AUDIO)
      return this;
    return nullptr;
  }

  // media::DemuxerStream implementation.
  void Read(const ReadCB& read_cb) override {}

  media::AudioDecoderConfig audio_decoder_config() override {
    media::AudioDecoderConfig config;
    config.Initialize(media::kCodecAAC,
                      media::kSampleFormatU8,
                      media::CHANNEL_LAYOUT_SURROUND,
                      48000,
                      NULL,
                      0,
                      false,
                      false,
                      base::TimeDelta(),
                      0);
    return config;
  }

  media::VideoDecoderConfig video_decoder_config() override {
    NOTREACHED();
    return media::VideoDecoderConfig();
  }

  media::DemuxerStream::Type type() const override {
    return media::DemuxerStream::AUDIO;
  }

  void EnableBitstreamConverter() override {}

  bool SupportsConfigChanges() override { return true; }

  media::VideoRotation video_rotation() override {
    NOTREACHED();
    return media::VIDEO_ROTATION_0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStream);
};

}  // namespace

namespace media {

class MojoRendererTest : public mojo::test::ApplicationTestBase {
 public:
  MojoRendererTest() : service_provider_(NULL) {}
  ~MojoRendererTest() override {}

 protected:
  // ApplicationTestBase implementation.
  mojo::ApplicationDelegate* GetApplicationDelegate() override {
    return &mojo_renderer_test_helper_;
  }

  void SetUp() override {
    ApplicationTestBase::SetUp();
    demuxer_stream_provider_.reset(new FakeDemuxerStream());
    service_provider_ =
        application_impl()
            ->ConnectToApplication("mojo:media")
            ->GetServiceProvider();
  }

  mojo::MediaRendererPtr CreateMediaRenderer() {
    mojo::MediaRendererPtr mojo_media_renderer;
    mojo::ConnectToService(service_provider_,
                           &mojo_media_renderer);
    return mojo_media_renderer.Pass();
  }

  DemuxerStreamProvider* stream_provider() {
    return demuxer_stream_provider_.get();
  }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return base::MessageLoop::current()->task_runner();
  }

 private:
  MojoRendererTestHelper mojo_renderer_test_helper_;
  scoped_ptr<DemuxerStreamProvider> demuxer_stream_provider_;
  mojo::ServiceProvider* service_provider_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererTest);
};

void ErrorCallback(PipelineStatus* output, PipelineStatus status) {
  *output = status;
}

// Tests that a MojoRendererImpl can successfully establish communication
// with a MojoRendererService and set up a MojoDemuxerStream
// connection. The test also initializes a media::AudioRendererImpl which
// will error-out expectedly due to lack of support for decoder selection.
TEST_F(MojoRendererTest, BasicInitialize) {
  MojoRendererImpl mojo_renderer_impl(task_runner(), CreateMediaRenderer());
  PipelineStatus expected_error(PIPELINE_OK);
  mojo_renderer_impl.Initialize(
      stream_provider(), base::MessageLoop::current()->QuitClosure(),
      media::StatisticsCB(), media::BufferingStateCB(),
      media::Renderer::PaintCB(), base::Closure(),
      base::Bind(&ErrorCallback, &expected_error));
  base::MessageLoop::current()->Run();
  EXPECT_EQ(PIPELINE_OK, expected_error);
}

}  // namespace media
