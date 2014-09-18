// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/sample_format.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/services/mojo_renderer_impl.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This class is here to give the gtest class access to the
// mojo::ApplicationImpl so that the tests can connect to other applications.
class MojoRendererTestHelper : public mojo::ApplicationDelegate {
 public:
  MojoRendererTestHelper() : application_impl_(NULL) {}
  virtual ~MojoRendererTestHelper() {}

  // ApplicationDelegate implementation.
  virtual void Initialize(mojo::ApplicationImpl* app) OVERRIDE {
    application_impl_ = app;
  }

  mojo::ApplicationImpl* application_impl() { return application_impl_; }

 private:
  mojo::ApplicationImpl* application_impl_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererTestHelper);
};

// TODO(tim): Reconcile this with mojo apptest framework when ready.
MojoRendererTestHelper* g_test_delegate = NULL;

// TODO(tim): Make media::FakeDemuxerStream support audio and use that for the
// DemuxerStream implementation instead.
class FakeDemuxerStream : public media::DemuxerStreamProvider,
                          public media::DemuxerStream {
 public:
  FakeDemuxerStream() {}
  virtual ~FakeDemuxerStream() {}

  // media::Demuxer implementation.
  virtual media::DemuxerStream* GetStream(
      media::DemuxerStream::Type type) OVERRIDE {
    DCHECK_EQ(media::DemuxerStream::AUDIO, type);
    return this;
  }
  virtual media::DemuxerStreamProvider::Liveness GetLiveness() const OVERRIDE {
    return media::DemuxerStreamProvider::LIVENESS_UNKNOWN;
  }

  // media::DemuxerStream implementation.
  virtual void Read(const ReadCB& read_cb) OVERRIDE {}

  virtual media::AudioDecoderConfig audio_decoder_config() OVERRIDE {
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

  virtual media::VideoDecoderConfig video_decoder_config() OVERRIDE {
    NOTREACHED();
    return media::VideoDecoderConfig();
  }

  virtual media::DemuxerStream::Type type() OVERRIDE {
    return media::DemuxerStream::AUDIO;
  }

  virtual void EnableBitstreamConverter() OVERRIDE {}

  virtual bool SupportsConfigChanges() OVERRIDE { return true; }

  virtual media::VideoRotation video_rotation() OVERRIDE {
    NOTREACHED();
    return media::VIDEO_ROTATION_0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStream);
};

}  // namespace

namespace media {

class MojoRendererTest : public testing::Test {
 public:
  MojoRendererTest() : service_provider_(NULL) {}

  virtual void SetUp() OVERRIDE {
    demuxer_stream_provider_.reset(new FakeDemuxerStream());
    service_provider_ =
        g_test_delegate->application_impl()
            ->ConnectToApplication("mojo:media_mojo_renderer_app")
            ->GetServiceProvider();
  }

  mojo::ServiceProvider* service_provider() { return service_provider_; }
  DemuxerStreamProvider* stream_provider() {
    return demuxer_stream_provider_.get();
  }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return base::MessageLoop::current()->task_runner();
  }

 private:
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
  MojoRendererImpl rimpl(task_runner(), stream_provider(), service_provider());
  PipelineStatus expected_error(PIPELINE_OK);
  rimpl.Initialize(base::MessageLoop::current()->QuitClosure(),
                   media::StatisticsCB(),
                   base::Closure(),
                   base::Bind(&ErrorCallback, &expected_error),
                   media::BufferingStateCB());
  base::MessageLoop::current()->Run();

  // We expect an error during initialization because MojoRendererService
  // doesn't initialize any decoders, which causes an error.
  EXPECT_EQ(PIPELINE_ERROR_COULD_NOT_RENDER, expected_error);
}

}  // namespace media

MojoResult MojoMain(MojoHandle shell_handle) {
  base::CommandLine::Init(0, NULL);
#if !defined(COMPONENT_BUILD)
  base::AtExitManager at_exit;
#endif

  // TODO(tim): Reconcile this with apptest framework when it is ready.
  scoped_ptr<mojo::ApplicationDelegate> delegate(new MojoRendererTestHelper());
  g_test_delegate = static_cast<MojoRendererTestHelper*>(delegate.get());
  {
    base::MessageLoop loop;
    mojo::ApplicationImpl impl(
        delegate.get(),
        mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)));

    int argc = 0;
    char** argv = NULL;
    testing::InitGoogleTest(&argc, argv);
    mojo_ignore_result(RUN_ALL_TESTS());
  }

  g_test_delegate = NULL;
  delegate.reset();
  return MOJO_RESULT_OK;
}
