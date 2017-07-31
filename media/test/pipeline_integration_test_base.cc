// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/test/pipeline_integration_test_base.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "media/base/media_log.h"
#include "media/base/media_tracks.h"
#include "media/base/test_data_util.h"
#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#endif
#include "media/filters/file_data_source.h"
#include "media/filters/memory_data_source.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/renderer_impl.h"
#include "media/test/fake_encrypted_media.h"
#include "media/test/mock_media_source.h"
#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

static std::vector<std::unique_ptr<VideoDecoder>> CreateVideoDecodersForTest(
    MediaLog* media_log,
    CreateVideoDecodersCB prepend_video_decoders_cb) {
  std::vector<std::unique_ptr<VideoDecoder>> video_decoders;

  if (!prepend_video_decoders_cb.is_null()) {
    video_decoders = prepend_video_decoders_cb.Run();
    DCHECK(!video_decoders.empty());
  }

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(base::MakeUnique<VpxVideoDecoder>());
#endif  // !defined(MEDIA_DISABLE_LIBVPX)

// Android does not have an ffmpeg video decoder.
#if !defined(MEDIA_DISABLE_FFMPEG) && !defined(OS_ANDROID) && \
    !defined(DISABLE_FFMPEG_VIDEO_DECODERS)
  video_decoders.push_back(base::MakeUnique<FFmpegVideoDecoder>(media_log));
#endif
  return video_decoders;
}

static std::vector<std::unique_ptr<AudioDecoder>> CreateAudioDecodersForTest(
    MediaLog* media_log,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  std::vector<std::unique_ptr<AudioDecoder>> audio_decoders;

  if (!prepend_audio_decoders_cb.is_null()) {
    audio_decoders = prepend_audio_decoders_cb.Run();
    DCHECK(!audio_decoders.empty());
  }

#if !defined(MEDIA_DISABLE_FFMPEG)
  audio_decoders.push_back(
      base::MakeUnique<FFmpegAudioDecoder>(media_task_runner, media_log));
#endif
  return audio_decoders;
}

const char kNullVideoHash[] = "d41d8cd98f00b204e9800998ecf8427e";
const char kNullAudioHash[] = "0.00,0.00,0.00,0.00,0.00,0.00,";

class RendererFactoryImpl final : public PipelineTestRendererFactory {
 public:
  explicit RendererFactoryImpl(PipelineIntegrationTestBase* integration_test)
      : integration_test_(integration_test) {}
  ~RendererFactoryImpl() override {}

  // PipelineTestRendererFactory implementation.
  std::unique_ptr<Renderer> CreateRenderer(
      CreateVideoDecodersCB prepend_video_decoders_cb,
      CreateAudioDecodersCB prepend_audio_decoders_cb) override {
    return integration_test_->CreateRenderer(prepend_video_decoders_cb,
                                             prepend_audio_decoders_cb);
  }

 private:
  PipelineIntegrationTestBase* integration_test_;

  DISALLOW_COPY_AND_ASSIGN(RendererFactoryImpl);
};

PipelineIntegrationTestBase::PipelineIntegrationTestBase()
    : hashing_enabled_(false),
      clockless_playback_(false),
      pipeline_(
          new PipelineImpl(scoped_task_environment_.GetMainThreadTaskRunner(),
                           &media_log_)),
      ended_(false),
      pipeline_status_(PIPELINE_OK),
      last_video_frame_format_(PIXEL_FORMAT_UNKNOWN),
      last_video_frame_color_space_(COLOR_SPACE_UNSPECIFIED),
      current_duration_(kInfiniteDuration),
      renderer_factory_(new RendererFactoryImpl(this)) {
  ResetVideoHash();
  EXPECT_CALL(*this, OnVideoAverageKeyframeDistanceUpdate()).Times(AnyNumber());
}

PipelineIntegrationTestBase::~PipelineIntegrationTestBase() {
  if (pipeline_->IsRunning())
    Stop();

  demuxer_.reset();
  pipeline_.reset();
  base::RunLoop().RunUntilIdle();
}

// TODO(xhwang): Method definitions in this file needs to be reordered.

void PipelineIntegrationTestBase::OnSeeked(base::TimeDelta seek_time,
                                           PipelineStatus status) {
  EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  pipeline_status_ = status;
}

void PipelineIntegrationTestBase::OnStatusCallback(PipelineStatus status) {
  pipeline_status_ = status;
  scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

void PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB(
    EmeInitDataType type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(!init_data.empty());
  CHECK(!encrypted_media_init_data_cb_.is_null());
  encrypted_media_init_data_cb_.Run(type, init_data);
}

void PipelineIntegrationTestBase::DemuxerMediaTracksUpdatedCB(
    std::unique_ptr<MediaTracks> tracks) {
  CHECK(tracks);
  CHECK_GT(tracks->tracks().size(), 0u);

  // Verify that track ids are unique.
  std::set<MediaTrack::Id> track_ids;
  for (const auto& track : tracks->tracks()) {
    EXPECT_EQ(track_ids.end(), track_ids.find(track->id()));
    track_ids.insert(track->id());
  }
}

void PipelineIntegrationTestBase::OnEnded() {
  DCHECK(!ended_);
  ended_ = true;
  pipeline_status_ = PIPELINE_OK;
  scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

bool PipelineIntegrationTestBase::WaitUntilOnEnded() {
  if (!ended_)
    base::RunLoop().Run();
  EXPECT_TRUE(ended_);
  scoped_task_environment_.RunUntilIdle();
  return ended_ && (pipeline_status_ == PIPELINE_OK);
}

PipelineStatus PipelineIntegrationTestBase::WaitUntilEndedOrError() {
  if (!ended_ && pipeline_status_ == PIPELINE_OK)
    base::RunLoop().Run();
  scoped_task_environment_.RunUntilIdle();
  return pipeline_status_;
}

void PipelineIntegrationTestBase::OnError(PipelineStatus status) {
  DCHECK_NE(status, PIPELINE_OK);
  pipeline_status_ = status;
  scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

PipelineStatus PipelineIntegrationTestBase::StartInternal(
    std::unique_ptr<DataSource> data_source,
    CdmContext* cdm_context,
    uint8_t test_type,
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  hashing_enabled_ = test_type & kHashed;
  clockless_playback_ = test_type & kClockless;

  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  // If the test is expected to have reliable duration information, permit at
  // most two calls to OnDurationChange.  CheckDuration will make sure that no
  // more than one of them is a finite duration.  This allows the pipeline to
  // call back at the end of the media with the known duration.
  //
  // In the event of unreliable duration information, just set the expectation
  // that it's called at least once. Such streams may repeatedly update their
  // duration as new packets are demuxed.
  if (test_type & kUnreliableDuration) {
    EXPECT_CALL(*this, OnDurationChange()).Times(AtLeast(1));
  } else {
    EXPECT_CALL(*this, OnDurationChange())
        .Times(AtMost(2))
        .WillRepeatedly(
            Invoke(this, &PipelineIntegrationTestBase::CheckDuration));
  }
  EXPECT_CALL(*this, OnVideoNaturalSizeChange(_)).Times(AtMost(1));
  EXPECT_CALL(*this, OnVideoOpacityChange(_)).WillRepeatedly(Return());
  CreateDemuxer(std::move(data_source));

  if (cdm_context) {
    EXPECT_CALL(*this, DecryptorAttached(true));
    pipeline_->SetCdm(
        cdm_context, base::Bind(&PipelineIntegrationTestBase::DecryptorAttached,
                                base::Unretained(this)));
  }

  // Should never be called as the required decryption keys for the encrypted
  // media files are provided in advance.
  EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);

  // SRC= demuxer does not support config changes.
  for (auto* stream : demuxer_->GetAllStreams()) {
    EXPECT_FALSE(stream->SupportsConfigChanges());
  }
  EXPECT_CALL(*this, OnAudioConfigChange(_)).Times(0);
  EXPECT_CALL(*this, OnVideoConfigChange(_)).Times(0);

  pipeline_->Start(demuxer_.get(),
                   renderer_factory_->CreateRenderer(prepend_video_decoders_cb,
                                                     prepend_audio_decoders_cb),
                   this,
                   base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                              base::Unretained(this)));
  base::RunLoop().Run();
  scoped_task_environment_.RunUntilIdle();
  return pipeline_status_;
}

PipelineStatus PipelineIntegrationTestBase::StartWithFile(
    const std::string& filename,
    CdmContext* cdm_context,
    uint8_t test_type,
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  std::unique_ptr<FileDataSource> file_data_source(new FileDataSource());
  base::FilePath file_path(GetTestDataFilePath(filename));
  CHECK(file_data_source->Initialize(file_path)) << "Is " << file_path.value()
                                                 << " missing?";
  return StartInternal(std::move(file_data_source), cdm_context, test_type,
                       prepend_video_decoders_cb, prepend_audio_decoders_cb);
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename) {
  return StartWithFile(filename, nullptr, kNormal);
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename,
                                                  CdmContext* cdm_context) {
  return StartWithFile(filename, cdm_context, kNormal);
}

PipelineStatus PipelineIntegrationTestBase::Start(
    const std::string& filename,
    uint8_t test_type,
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  return StartWithFile(filename, nullptr, test_type, prepend_video_decoders_cb,
                       prepend_audio_decoders_cb);
}

PipelineStatus PipelineIntegrationTestBase::Start(const uint8_t* data,
                                                  size_t size,
                                                  uint8_t test_type) {
  return StartInternal(base::MakeUnique<MemoryDataSource>(data, size), nullptr,
                       test_type);
}

void PipelineIntegrationTestBase::Play() {
  pipeline_->SetPlaybackRate(1);
}

void PipelineIntegrationTestBase::Pause() {
  pipeline_->SetPlaybackRate(0);
}

bool PipelineIntegrationTestBase::Seek(base::TimeDelta seek_time) {
  ended_ = false;

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  pipeline_->Seek(seek_time, base::Bind(&PipelineIntegrationTestBase::OnSeeked,
                                        base::Unretained(this), seek_time));
  run_loop.Run();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_CALL(*this, OnBufferingStateChange(_)).Times(AnyNumber());
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Suspend() {
  pipeline_->Suspend(base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                                base::Unretained(this)));
  base::RunLoop().Run();
  scoped_task_environment_.RunUntilIdle();
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Resume(base::TimeDelta seek_time) {
  ended_ = false;

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  pipeline_->Resume(renderer_factory_->CreateRenderer(CreateVideoDecodersCB(),
                                                      CreateAudioDecodersCB()),
                    seek_time,
                    base::Bind(&PipelineIntegrationTestBase::OnSeeked,
                               base::Unretained(this), seek_time));
  run_loop.Run();
  scoped_task_environment_.RunUntilIdle();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Stop() {
  DCHECK(pipeline_->IsRunning());
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

void PipelineIntegrationTestBase::FailTest(PipelineStatus status) {
  DCHECK_NE(PIPELINE_OK, status);
  OnError(status);
}

void PipelineIntegrationTestBase::QuitAfterCurrentTimeTask(
    base::TimeDelta quit_time,
    base::OnceClosure quit_closure) {
  if (pipeline_->GetMediaTime() >= quit_time ||
      pipeline_status_ != PIPELINE_OK) {
    std::move(quit_closure).Run();
    return;
  }

  scoped_task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                     base::Unretained(this), quit_time,
                     std::move(quit_closure)),
      base::TimeDelta::FromMilliseconds(10));
}

bool PipelineIntegrationTestBase::WaitUntilCurrentTimeIsAfter(
    const base::TimeDelta& wait_time) {
  DCHECK(pipeline_->IsRunning());
  DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
  DCHECK(wait_time <= pipeline_->GetMediaDuration());

  base::RunLoop run_loop;
  scoped_task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                     base::Unretained(this), wait_time,
                     run_loop.QuitWhenIdleClosure()),
      base::TimeDelta::FromMilliseconds(10));
  run_loop.Run();
  scoped_task_environment_.RunUntilIdle();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::CreateDemuxer(
    std::unique_ptr<DataSource> data_source) {
  data_source_ = std::move(data_source);

#if !defined(MEDIA_DISABLE_FFMPEG)
  demuxer_ = std::unique_ptr<Demuxer>(new FFmpegDemuxer(
      scoped_task_environment_.GetMainThreadTaskRunner(), data_source_.get(),
      base::Bind(&PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::DemuxerMediaTracksUpdatedCB,
                 base::Unretained(this)),
      &media_log_));
#endif
}

std::unique_ptr<Renderer> PipelineIntegrationTestBase::CreateRenderer(
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  // Simulate a 60Hz rendering sink.
  video_sink_.reset(new NullVideoSink(
      clockless_playback_, base::TimeDelta::FromSecondsD(1.0 / 60),
      base::Bind(&PipelineIntegrationTestBase::OnVideoFramePaint,
                 base::Unretained(this)),
      scoped_task_environment_.GetMainThreadTaskRunner()));

  // Disable frame dropping if hashing is enabled.
  std::unique_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner().get(),
      video_sink_.get(),
      base::Bind(&CreateVideoDecodersForTest, &media_log_,
                 prepend_video_decoders_cb),
      false, nullptr, &media_log_));

  if (!clockless_playback_) {
    audio_sink_ =
        new NullAudioSink(scoped_task_environment_.GetMainThreadTaskRunner());
  } else {
    clockless_audio_sink_ = new ClocklessAudioSink(OutputDeviceInfo(
        "", OUTPUT_DEVICE_STATUS_OK,
        // Don't allow the audio renderer to resample buffers if hashing is
        // enabled:
        hashing_enabled_
            ? AudioParameters()
            : AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                              CHANNEL_LAYOUT_STEREO, 44100, 16, 512)));
  }

  std::unique_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      scoped_task_environment_.GetMainThreadTaskRunner(),
      (clockless_playback_)
          ? static_cast<AudioRendererSink*>(clockless_audio_sink_.get())
          : audio_sink_.get(),
      base::Bind(&CreateAudioDecodersForTest, &media_log_,
                 scoped_task_environment_.GetMainThreadTaskRunner(),
                 prepend_audio_decoders_cb),
      &media_log_));
  if (hashing_enabled_) {
    if (clockless_playback_)
      clockless_audio_sink_->StartAudioHashForTesting();
    else
      audio_sink_->StartAudioHashForTesting();
  }

  std::unique_ptr<RendererImpl> renderer_impl(
      new RendererImpl(scoped_task_environment_.GetMainThreadTaskRunner(),
                       std::move(audio_renderer), std::move(video_renderer)));

  // Prevent non-deterministic buffering state callbacks from firing (e.g., slow
  // machine, valgrind).
  renderer_impl->DisableUnderflowForTesting();

  if (clockless_playback_)
    renderer_impl->EnableClocklessVideoPlaybackForTesting();

  return std::move(renderer_impl);
}

void PipelineIntegrationTestBase::OnVideoFramePaint(
    const scoped_refptr<VideoFrame>& frame) {
  last_video_frame_format_ = frame->format();
  int result;
  if (frame->metadata()->GetInteger(VideoFrameMetadata::COLOR_SPACE, &result))
    last_video_frame_color_space_ = static_cast<ColorSpace>(result);
  if (!hashing_enabled_ || last_frame_ == frame)
    return;
  last_frame_ = frame;
  DVLOG(3) << __func__ << " pts=" << frame->timestamp().InSecondsF();
  VideoFrame::HashFrameForTesting(&md5_context_, frame);
}

void PipelineIntegrationTestBase::CheckDuration() {
  // Allow the pipeline to specify indefinite duration, then reduce it once
  // it becomes known.
  ASSERT_EQ(kInfiniteDuration, current_duration_);
  base::TimeDelta new_duration = pipeline_->GetMediaDuration();
  current_duration_ = new_duration;
}

base::TimeDelta PipelineIntegrationTestBase::GetStartTime() {
  return demuxer_->GetStartTime();
}

void PipelineIntegrationTestBase::ResetVideoHash() {
  DVLOG(1) << __func__;
  base::MD5Init(&md5_context_);
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

PipelineStatus PipelineIntegrationTestBase::StartPipelineWithMediaSource(
    MockMediaSource* source) {
  return StartPipelineWithMediaSource(source, kNormal, nullptr);
}

PipelineStatus PipelineIntegrationTestBase::StartPipelineWithEncryptedMedia(
    MockMediaSource* source,
    FakeEncryptedMedia* encrypted_media) {
  return StartPipelineWithMediaSource(source, kNormal, encrypted_media);
}

PipelineStatus PipelineIntegrationTestBase::StartPipelineWithMediaSource(
    MockMediaSource* source,
    uint8_t test_type,
    FakeEncryptedMedia* encrypted_media) {
  hashing_enabled_ = test_type & kHashed;
  clockless_playback_ = test_type & kClockless;

  if (!(test_type & kExpectDemuxerFailure))
    EXPECT_CALL(*source, InitSegmentReceivedMock(_)).Times(AtLeast(1));

  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnDurationChange()).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoNaturalSizeChange(_)).Times(AtMost(1));
  EXPECT_CALL(*this, OnVideoOpacityChange(_)).Times(AtMost(1));

  source->set_demuxer_failure_cb(base::Bind(
      &PipelineIntegrationTestBase::OnStatusCallback, base::Unretained(this)));
  demuxer_ = source->GetDemuxer();

  // MediaSource demuxer may signal config changes.
  for (auto* stream : demuxer_->GetAllStreams()) {
    EXPECT_TRUE(stream->SupportsConfigChanges());
  }
  // Config change tests should set more specific expectations about the number
  // of calls.
  EXPECT_CALL(*this, OnAudioConfigChange(_)).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoConfigChange(_)).Times(AnyNumber());

  if (encrypted_media) {
    EXPECT_CALL(*this, DecryptorAttached(true));

    // Encrypted content used but keys provided in advance, so this is
    // never called.
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);
    pipeline_->SetCdm(
        encrypted_media->GetCdmContext(),
        base::Bind(&PipelineIntegrationTestBase::DecryptorAttached,
                   base::Unretained(this)));
  } else {
    // Encrypted content not used, so this is never called.
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);
  }

  pipeline_->Start(demuxer_.get(),
                   renderer_factory_->CreateRenderer(CreateVideoDecodersCB(),
                                                     CreateAudioDecodersCB()),
                   this,
                   base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                              base::Unretained(this)));

  if (encrypted_media) {
    source->set_encrypted_media_init_data_cb(
        base::Bind(&FakeEncryptedMedia::OnEncryptedMediaInitData,
                   base::Unretained(encrypted_media)));
  }
  base::RunLoop().Run();
  scoped_task_environment_.RunUntilIdle();
  return pipeline_status_;
}

base::TimeTicks DummyTickClock::NowTicks() {
  now_ += base::TimeDelta::FromSeconds(60);
  return now_;
}

}  // namespace media
