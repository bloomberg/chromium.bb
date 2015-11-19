// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/test/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "media/base/cdm_context.h"
#include "media/base/media_log.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer.h"
#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#endif
#include "media/filters/file_data_source.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/renderer_impl.h"
#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace media {

const char kNullVideoHash[] = "d41d8cd98f00b204e9800998ecf8427e";
const char kNullAudioHash[] = "0.00,0.00,0.00,0.00,0.00,0.00,";

PipelineIntegrationTestBase::PipelineIntegrationTestBase()
    : hashing_enabled_(false),
      clockless_playback_(false),
      pipeline_(new Pipeline(message_loop_.task_runner(), new MediaLog())),
      ended_(false),
      pipeline_status_(PIPELINE_OK),
      last_video_frame_format_(PIXEL_FORMAT_UNKNOWN),
      last_video_frame_color_space_(COLOR_SPACE_UNSPECIFIED),
      hardware_config_(AudioParameters(), AudioParameters()) {
  base::MD5Init(&md5_context_);
}

PipelineIntegrationTestBase::~PipelineIntegrationTestBase() {
  if (!pipeline_->IsRunning())
    return;

  Stop();
}

void PipelineIntegrationTestBase::OnSeeked(base::TimeDelta seek_time,
                                           PipelineStatus status) {
  EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  pipeline_status_ = status;
}

void PipelineIntegrationTestBase::OnStatusCallback(
    PipelineStatus status) {
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

void PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB(
    EmeInitDataType type,
    const std::vector<uint8>& init_data) {
  DCHECK(!init_data.empty());
  CHECK(!encrypted_media_init_data_cb_.is_null());
  encrypted_media_init_data_cb_.Run(type, init_data);
}

void PipelineIntegrationTestBase::OnEnded() {
  DCHECK(!ended_);
  ended_ = true;
  pipeline_status_ = PIPELINE_OK;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

bool PipelineIntegrationTestBase::WaitUntilOnEnded() {
  if (ended_)
    return (pipeline_status_ == PIPELINE_OK);
  message_loop_.Run();
  EXPECT_TRUE(ended_);
  return ended_ && (pipeline_status_ == PIPELINE_OK);
}

PipelineStatus PipelineIntegrationTestBase::WaitUntilEndedOrError() {
  if (ended_ || pipeline_status_ != PIPELINE_OK)
    return pipeline_status_;
  message_loop_.Run();
  return pipeline_status_;
}

void PipelineIntegrationTestBase::OnError(PipelineStatus status) {
  DCHECK_NE(status, PIPELINE_OK);
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename) {
  return Start(filename, nullptr);
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename,
                                                  CdmContext* cdm_context) {
  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  CreateDemuxer(filename);

  if (cdm_context) {
    EXPECT_CALL(*this, DecryptorAttached(true));
    pipeline_->SetCdm(
        cdm_context, base::Bind(&PipelineIntegrationTestBase::DecryptorAttached,
                                base::Unretained(this)));
  }

  // Should never be called as the required decryption keys for the encrypted
  // media files are provided in advance.
  EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);

  pipeline_->Start(
      demuxer_.get(), CreateRenderer(),
      base::Bind(&PipelineIntegrationTestBase::OnEnded, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnError, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnMetadata,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnBufferingStateChanged,
                 base::Unretained(this)),
      base::Closure(), base::Bind(&PipelineIntegrationTestBase::OnAddTextTrack,
                                  base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnWaitingForDecryptionKey,
                 base::Unretained(this)));
  message_loop_.Run();
  return pipeline_status_;
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename,
                                                  uint8_t test_type) {
  hashing_enabled_ = test_type & kHashed;
  clockless_playback_ = test_type & kClockless;
  return Start(filename);
}

void PipelineIntegrationTestBase::Play() {
  pipeline_->SetPlaybackRate(1);
}

void PipelineIntegrationTestBase::Pause() {
  pipeline_->SetPlaybackRate(0);
}

bool PipelineIntegrationTestBase::Seek(base::TimeDelta seek_time) {
  ended_ = false;

  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&message_loop_, &base::MessageLoop::QuitNow));
  pipeline_->Seek(seek_time, base::Bind(&PipelineIntegrationTestBase::OnSeeked,
                                        base::Unretained(this), seek_time));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Stop() {
  DCHECK(pipeline_->IsRunning());
  pipeline_->Stop(base::MessageLoop::QuitWhenIdleClosure());
  message_loop_.Run();
}

void PipelineIntegrationTestBase::QuitAfterCurrentTimeTask(
    const base::TimeDelta& quit_time) {
  if (pipeline_->GetMediaTime() >= quit_time ||
      pipeline_status_ != PIPELINE_OK) {
    message_loop_.QuitWhenIdle();
    return;
  }

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this), quit_time),
      base::TimeDelta::FromMilliseconds(10));
}

bool PipelineIntegrationTestBase::WaitUntilCurrentTimeIsAfter(
    const base::TimeDelta& wait_time) {
  DCHECK(pipeline_->IsRunning());
  DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
  DCHECK(wait_time <= pipeline_->GetMediaDuration());

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this),
                 wait_time),
      base::TimeDelta::FromMilliseconds(10));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::CreateDemuxer(const std::string& filename) {
  FileDataSource* file_data_source = new FileDataSource();
  base::FilePath file_path(GetTestDataFilePath(filename));
  CHECK(file_data_source->Initialize(file_path)) << "Is " << file_path.value()
                                                 << " missing?";
  data_source_.reset(file_data_source);

  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
      base::Bind(&PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB,
                 base::Unretained(this));

#if !defined(MEDIA_DISABLE_FFMPEG)
  demuxer_ = scoped_ptr<Demuxer>(
      new FFmpegDemuxer(message_loop_.task_runner(), data_source_.get(),
                        encrypted_media_init_data_cb, new MediaLog()));
#endif
}

scoped_ptr<Renderer> PipelineIntegrationTestBase::CreateRenderer() {
  ScopedVector<VideoDecoder> video_decoders;
#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(
      new VpxVideoDecoder(message_loop_.task_runner()));
#endif  // !defined(MEDIA_DISABLE_LIBVPX)

#if !defined(MEDIA_DISABLE_FFMPEG)
  video_decoders.push_back(
      new FFmpegVideoDecoder(message_loop_.task_runner()));
#endif

  // Simulate a 60Hz rendering sink.
  video_sink_.reset(new NullVideoSink(
      clockless_playback_, base::TimeDelta::FromSecondsD(1.0 / 60),
      base::Bind(&PipelineIntegrationTestBase::OnVideoFramePaint,
                 base::Unretained(this)),
      message_loop_.task_runner()));

  // Disable frame dropping if hashing is enabled.
  scoped_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      message_loop_.task_runner(), message_loop_.task_runner().get(),
      video_sink_.get(), video_decoders.Pass(), false, nullptr,
      new MediaLog()));

  if (!clockless_playback_) {
    audio_sink_ = new NullAudioSink(message_loop_.task_runner());
  } else {
    clockless_audio_sink_ = new ClocklessAudioSink();
  }

  ScopedVector<AudioDecoder> audio_decoders;

#if !defined(MEDIA_DISABLE_FFMPEG)
  audio_decoders.push_back(
      new FFmpegAudioDecoder(message_loop_.task_runner(), new MediaLog()));
#endif

  audio_decoders.push_back(
      new OpusAudioDecoder(message_loop_.task_runner()));

  // Don't allow the audio renderer to resample buffers if hashing is enabled.
  if (!hashing_enabled_) {
    AudioParameters out_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               CHANNEL_LAYOUT_STEREO,
                               44100,
                               16,
                               512);
    hardware_config_.UpdateOutputConfig(out_params);
  }

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      message_loop_.task_runner(),
      (clockless_playback_)
          ? static_cast<AudioRendererSink*>(clockless_audio_sink_.get())
          : audio_sink_.get(),
      audio_decoders.Pass(), hardware_config_, new MediaLog()));
  if (hashing_enabled_) {
    if (clockless_playback_)
      clockless_audio_sink_->StartAudioHashForTesting();
    else
      audio_sink_->StartAudioHashForTesting();
  }

  scoped_ptr<RendererImpl> renderer_impl(
      new RendererImpl(message_loop_.task_runner(),
                       audio_renderer.Pass(),
                       video_renderer.Pass()));

  // Prevent non-deterministic buffering state callbacks from firing (e.g., slow
  // machine, valgrind).
  renderer_impl->DisableUnderflowForTesting();

  if (clockless_playback_)
    renderer_impl->EnableClocklessVideoPlaybackForTesting();

  return renderer_impl.Pass();
}

void PipelineIntegrationTestBase::OnVideoFramePaint(
    const scoped_refptr<VideoFrame>& frame) {
  last_video_frame_format_ = frame->format();
  int result;
  if (frame->metadata()->GetInteger(VideoFrameMetadata::COLOR_SPACE, &result))
    last_video_frame_color_space_ = static_cast<ColorSpace>(result);
  if (!hashing_enabled_)
    return;
  VideoFrame::HashFrameForTesting(&md5_context_, frame);
}

std::string PipelineIntegrationTestBase::GetVideoHash() {
  DCHECK(hashing_enabled_);
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  return base::MD5DigestToBase16(digest);
}

std::string PipelineIntegrationTestBase::GetAudioHash() {
  DCHECK(hashing_enabled_);

  if (clockless_playback_)
    return clockless_audio_sink_->GetAudioHashForTesting();
  return audio_sink_->GetAudioHashForTesting();
}

base::TimeDelta PipelineIntegrationTestBase::GetAudioTime() {
  DCHECK(clockless_playback_);
  return clockless_audio_sink_->render_time();
}

base::TimeTicks DummyTickClock::NowTicks() {
  now_ += base::TimeDelta::FromSeconds(60);
  return now_;
}

}  // namespace media
