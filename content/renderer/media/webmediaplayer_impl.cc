// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_impl.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/waitable_event.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/buffered_data_source.h"
#include "content/renderer/media/crypto/encrypted_media_player_support.h"
#include "content/renderer/media/render_media_log.h"
#include "content/renderer/media/texttrack_impl.h"
#include "content/renderer/media/webaudiosourceprovider_impl.h"
#include "content/renderer/media/webinbandtexttrack_impl.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/media/webmediaplayer_params.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "content/renderer/media/webmediasource_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/filter_collection.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/text_renderer.h"
#include "media/base/video_frame.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "media/filters/gpu_video_decoder.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/filters/renderer_impl.h"
#include "media/filters/video_renderer_impl.h"
#include "media/filters/vpx_video_decoder.h"
#include "third_party/WebKit/public/platform/WebMediaSource.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebCanvas;
using blink::WebMediaPlayer;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using media::PipelineStatus;

namespace {

// Limits the range of playback rate.
//
// TODO(kylep): Revisit these.
//
// Vista has substantially lower performance than XP or Windows7.  If you speed
// up a video too much, it can't keep up, and rendering stops updating except on
// the time bar. For really high speeds, audio becomes a bottleneck and we just
// use up the data we have, which may not achieve the speed requested, but will
// not crash the tab.
//
// A very slow speed, ie 0.00000001x, causes the machine to lock up. (It seems
// like a busy loop). It gets unresponsive, although its not completely dead.
//
// Also our timers are not very accurate (especially for ogg), which becomes
// evident at low speeds and on Vista. Since other speeds are risky and outside
// the norms, we think 1/16x to 16x is a safe and useful range for now.
const double kMinRate = 0.0625;
const double kMaxRate = 16.0;

class SyncPointClientImpl : public media::VideoFrame::SyncPointClient {
 public:
  explicit SyncPointClientImpl(
      blink::WebGraphicsContext3D* web_graphics_context)
      : web_graphics_context_(web_graphics_context) {}
  virtual ~SyncPointClientImpl() {}
  virtual uint32 InsertSyncPoint() OVERRIDE {
    return web_graphics_context_->insertSyncPoint();
  }
  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE {
    web_graphics_context_->waitSyncPoint(sync_point);
  }

 private:
  blink::WebGraphicsContext3D* web_graphics_context_;
};

}  // namespace

namespace content {

class BufferedDataSourceHostImpl;

#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(static_cast<int>(WebMediaPlayer::CORSMode ## name) == \
                 static_cast<int>(BufferedResourceLoader::k ## name), \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(Unspecified);
COMPILE_ASSERT_MATCHING_ENUM(Anonymous);
COMPILE_ASSERT_MATCHING_ENUM(UseCredentials);
#undef COMPILE_ASSERT_MATCHING_ENUM

#define BIND_TO_RENDER_LOOP(function) \
  (DCHECK(main_loop_->BelongsToCurrentThread()), \
  media::BindToCurrentLoop(base::Bind(function, AsWeakPtr())))

#define BIND_TO_RENDER_LOOP1(function, arg1) \
  (DCHECK(main_loop_->BelongsToCurrentThread()), \
  media::BindToCurrentLoop(base::Bind(function, AsWeakPtr(), arg1)))

static void LogMediaSourceError(const scoped_refptr<media::MediaLog>& media_log,
                                const std::string& error) {
  media_log->AddEvent(media_log->CreateMediaSourceErrorEvent(error));
}

WebMediaPlayerImpl::WebMediaPlayerImpl(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    const WebMediaPlayerParams& params)
    : frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      preload_(AUTO),
      main_loop_(base::MessageLoopProxy::current()),
      media_loop_(
          RenderThreadImpl::current()->GetMediaThreadMessageLoopProxy()),
      media_log_(new RenderMediaLog()),
      pipeline_(media_loop_, media_log_.get()),
      load_type_(LoadTypeURL),
      opaque_(false),
      paused_(true),
      seeking_(false),
      playback_rate_(0.0f),
      pending_seek_(false),
      pending_seek_seconds_(0.0f),
      should_notify_time_changed_(false),
      client_(client),
      delegate_(delegate),
      defer_load_cb_(params.defer_load_cb()),
      gpu_factories_(RenderThreadImpl::current()->GetGpuFactories()),
      supports_save_(true),
      chunk_demuxer_(NULL),
      // Threaded compositing isn't enabled universally yet.
      compositor_task_runner_(
          RenderThreadImpl::current()->compositor_message_loop_proxy()
              ? RenderThreadImpl::current()->compositor_message_loop_proxy()
              : base::MessageLoopProxy::current()),
      compositor_(new VideoFrameCompositor(
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnNaturalSizeChanged),
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnOpacityChanged))),
      text_track_index_(0),
      encrypted_media_support_(EncryptedMediaPlayerSupport::Create(client)) {
  DCHECK(encrypted_media_support_);

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));

  // |gpu_factories_| requires that its entry points be called on its
  // |GetTaskRunner()|.  Since |pipeline_| will own decoders created from the
  // factories, require that their message loops are identical.
  DCHECK(!gpu_factories_.get() ||
         (gpu_factories_->GetTaskRunner() == media_loop_.get()));

  // Use the null sink if no sink was provided.
  audio_source_provider_ = new WebAudioSourceProviderImpl(
      params.audio_renderer_sink().get()
          ? params.audio_renderer_sink()
          : new media::NullAudioSink(media_loop_));
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  client_->setWebLayer(NULL);

  DCHECK(main_loop_->BelongsToCurrentThread());
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (delegate_.get())
    delegate_->PlayerGone(this);

  // Abort any pending IO so stopping the pipeline doesn't get blocked.
  if (data_source_)
    data_source_->Abort();
  if (chunk_demuxer_) {
    chunk_demuxer_->Shutdown();
    chunk_demuxer_ = NULL;
  }

  gpu_factories_ = NULL;

  // Make sure to kill the pipeline so there's no more media threads running.
  // Note: stopping the pipeline might block for a long time.
  base::WaitableEvent waiter(false, false);
  pipeline_.Stop(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
  waiter.Wait();

  compositor_task_runner_->DeleteSoon(FROM_HERE, compositor_);
}

void WebMediaPlayerImpl::load(LoadType load_type, const blink::WebURL& url,
                              CORSMode cors_mode) {
  DVLOG(1) << __FUNCTION__ << "(" << load_type << ", " << url << ", "
           << cors_mode << ")";
  if (!defer_load_cb_.is_null()) {
    defer_load_cb_.Run(base::Bind(
        &WebMediaPlayerImpl::DoLoad, AsWeakPtr(), load_type, url, cors_mode));
    return;
  }
  DoLoad(load_type, url, cors_mode);
}

void WebMediaPlayerImpl::DoLoad(LoadType load_type,
                                const blink::WebURL& url,
                                CORSMode cors_mode) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  GURL gurl(url);
  ReportMediaSchemeUma(gurl);

  // Set subresource URL for crash reporting.
  base::debug::SetCrashKeyValue("subresource_url", gurl.spec());

  load_type_ = load_type;

  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  // Media source pipelines can start immediately.
  if (load_type == LoadTypeMediaSource) {
    supports_save_ = false;
    StartPipeline();
    return;
  }

  // Otherwise it's a regular request which requires resolving the URL first.
  data_source_.reset(new BufferedDataSource(
      url,
      static_cast<BufferedResourceLoader::CORSMode>(cors_mode),
      main_loop_,
      frame_,
      media_log_.get(),
      &buffered_data_source_host_,
      base::Bind(&WebMediaPlayerImpl::NotifyDownloading, AsWeakPtr())));
  data_source_->Initialize(
      base::Bind(&WebMediaPlayerImpl::DataSourceInitialized, AsWeakPtr()));
  data_source_->SetPreload(preload_);
}

void WebMediaPlayerImpl::play() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_loop_->BelongsToCurrentThread());

  paused_ = false;
  pipeline_.SetPlaybackRate(playback_rate_);
  if (data_source_)
    data_source_->MediaIsPlaying();

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));

  if (delegate_.get())
    delegate_->DidPlay(this);
}

void WebMediaPlayerImpl::pause() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_loop_->BelongsToCurrentThread());

  paused_ = true;
  pipeline_.SetPlaybackRate(0.0f);
  if (data_source_)
    data_source_->MediaIsPaused();
  paused_time_ = pipeline_.GetMediaTime();

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));

  if (delegate_.get())
    delegate_->DidPause(this);
}

bool WebMediaPlayerImpl::supportsSave() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return supports_save_;
}

void WebMediaPlayerImpl::seek(double seconds) {
  DVLOG(1) << __FUNCTION__ << "(" << seconds << ")";
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (ready_state_ > WebMediaPlayer::ReadyStateHaveMetadata)
    SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);

  if (seeking_) {
    pending_seek_ = true;
    pending_seek_seconds_ = seconds;
    if (chunk_demuxer_)
      chunk_demuxer_->CancelPendingSeek(seek_time);
    return;
  }

  media_log_->AddEvent(media_log_->CreateSeekEvent(seconds));

  // Update our paused time.
  if (paused_)
    paused_time_ = seek_time;

  seeking_ = true;

  if (chunk_demuxer_)
    chunk_demuxer_->StartWaitingForSeek(seek_time);

  // Kick off the asynchronous seek!
  pipeline_.Seek(
      seek_time,
      BIND_TO_RENDER_LOOP1(&WebMediaPlayerImpl::OnPipelineSeeked, true));
}

void WebMediaPlayerImpl::setRate(double rate) {
  DVLOG(1) << __FUNCTION__ << "(" << rate << ")";
  DCHECK(main_loop_->BelongsToCurrentThread());

  // TODO(kylep): Remove when support for negatives is added. Also, modify the
  // following checks so rewind uses reasonable values also.
  if (rate < 0.0)
    return;

  // Limit rates to reasonable values by clamping.
  if (rate != 0.0) {
    if (rate < kMinRate)
      rate = kMinRate;
    else if (rate > kMaxRate)
      rate = kMaxRate;
  }

  playback_rate_ = rate;
  if (!paused_) {
    pipeline_.SetPlaybackRate(rate);
    if (data_source_)
      data_source_->MediaPlaybackRateChanged(rate);
  }
}

void WebMediaPlayerImpl::setVolume(double volume) {
  DVLOG(1) << __FUNCTION__ << "(" << volume << ")";
  DCHECK(main_loop_->BelongsToCurrentThread());

  pipeline_.SetVolume(volume);
}

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, chromium_name) \
    COMPILE_ASSERT(static_cast<int>(WebMediaPlayer::webkit_name) == \
                   static_cast<int>(content::chromium_name), \
                   mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(PreloadNone, NONE);
COMPILE_ASSERT_MATCHING_ENUM(PreloadMetaData, METADATA);
COMPILE_ASSERT_MATCHING_ENUM(PreloadAuto, AUTO);
#undef COMPILE_ASSERT_MATCHING_ENUM

void WebMediaPlayerImpl::setPreload(WebMediaPlayer::Preload preload) {
  DVLOG(1) << __FUNCTION__ << "(" << preload << ")";
  DCHECK(main_loop_->BelongsToCurrentThread());

  preload_ = static_cast<content::Preload>(preload);
  if (data_source_)
    data_source_->SetPreload(preload_);
}

bool WebMediaPlayerImpl::hasVideo() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return pipeline_metadata_.has_video;
}

bool WebMediaPlayerImpl::hasAudio() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return pipeline_metadata_.has_audio;
}

blink::WebSize WebMediaPlayerImpl::naturalSize() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return blink::WebSize(pipeline_metadata_.natural_size);
}

bool WebMediaPlayerImpl::paused() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return pipeline_.GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::seeking() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return false;

  return seeking_;
}

double WebMediaPlayerImpl::duration() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  return GetPipelineDuration();
}

double WebMediaPlayerImpl::timelineOffset() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (pipeline_metadata_.timeline_offset.is_null())
    return std::numeric_limits<double>::quiet_NaN();

  return pipeline_metadata_.timeline_offset.ToJsTime();
}

double WebMediaPlayerImpl::currentTime() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return (paused_ ? paused_time_ : pipeline_.GetMediaTime()).InSecondsF();
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::networkState() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::readyState() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return ready_state_;
}

blink::WebTimeRanges WebMediaPlayerImpl::buffered() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::Ranges<base::TimeDelta> buffered_time_ranges =
      pipeline_.GetBufferedTimeRanges();

  const base::TimeDelta duration = pipeline_.GetMediaDuration();
  if (duration != media::kInfiniteDuration()) {
    buffered_data_source_host_.AddBufferedTimeRanges(
        &buffered_time_ranges, duration);
  }
  return ConvertToWebTimeRanges(buffered_time_ranges);
}

double WebMediaPlayerImpl::maxTimeSeekable() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // If we haven't even gotten to ReadyStateHaveMetadata yet then just
  // return 0 so that the seekable range is empty.
  if (ready_state_ < WebMediaPlayer::ReadyStateHaveMetadata)
    return 0.0;

  // We don't support seeking in streaming media.
  if (data_source_ && data_source_->IsStreaming())
    return 0.0;
  return duration();
}

bool WebMediaPlayerImpl::didLoadingProgress() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  bool pipeline_progress = pipeline_.DidLoadingProgress();
  bool data_progress = buffered_data_source_host_.DidLoadingProgress();
  return pipeline_progress || data_progress;
}

void WebMediaPlayerImpl::paint(blink::WebCanvas* canvas,
                               const blink::WebRect& rect,
                               unsigned char alpha) {
  paint(canvas, rect, alpha, SkXfermode::kSrcOver_Mode);
}

void WebMediaPlayerImpl::paint(blink::WebCanvas* canvas,
                               const blink::WebRect& rect,
                               unsigned char alpha,
                               SkXfermode::Mode mode) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl:paint");

  // TODO(scherkus): Clarify paint() API contract to better understand when and
  // why it's being called. For example, today paint() is called when:
  //   - We haven't reached HAVE_CURRENT_DATA and need to paint black
  //   - We're painting to a canvas
  // See http://crbug.com/341225 http://crbug.com/342621 for details.
  scoped_refptr<media::VideoFrame> video_frame =
      GetCurrentFrameFromCompositor();

  gfx::Rect gfx_rect(rect);

  skcanvas_video_renderer_.Paint(video_frame.get(),
                                 canvas,
                                 gfx_rect,
                                 alpha,
                                 mode,
                                 pipeline_metadata_.video_rotation);
}

bool WebMediaPlayerImpl::hasSingleSecurityOrigin() const {
  if (data_source_)
    return data_source_->HasSingleOrigin();
  return true;
}

bool WebMediaPlayerImpl::didPassCORSAccessCheck() const {
  if (data_source_)
    return data_source_->DidPassCORSAccessCheck();
  return false;
}

double WebMediaPlayerImpl::mediaTimeForTimeValue(double timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::decodedFrameCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.video_frames_decoded;
}

unsigned WebMediaPlayerImpl::droppedFrameCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.video_frames_dropped;
}

unsigned WebMediaPlayerImpl::audioDecodedByteCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.audio_bytes_decoded;
}

unsigned WebMediaPlayerImpl::videoDecodedByteCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.video_bytes_decoded;
}

bool WebMediaPlayerImpl::copyVideoTextureToPlatformTexture(
    blink::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  TRACE_EVENT0("media", "WebMediaPlayerImpl:copyVideoTextureToPlatformTexture");

  scoped_refptr<media::VideoFrame> video_frame =
      GetCurrentFrameFromCompositor();

  if (!video_frame.get())
    return false;
  if (video_frame->format() != media::VideoFrame::NATIVE_TEXTURE)
    return false;

  const gpu::MailboxHolder* mailbox_holder = video_frame->mailbox_holder();
  if (mailbox_holder->texture_target != GL_TEXTURE_2D)
    return false;

  web_graphics_context->waitSyncPoint(mailbox_holder->sync_point);
  uint32 source_texture = web_graphics_context->createAndConsumeTextureCHROMIUM(
      GL_TEXTURE_2D, mailbox_holder->mailbox.name);

  // The video is stored in a unmultiplied format, so premultiply
  // if necessary.
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    premultiply_alpha);
  // Application itself needs to take care of setting the right flip_y
  // value down to get the expected result.
  // flip_y==true means to reverse the video orientation while
  // flip_y==false means to keep the intrinsic orientation.
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
  web_graphics_context->copyTextureCHROMIUM(GL_TEXTURE_2D,
                                            source_texture,
                                            texture,
                                            level,
                                            internal_format,
                                            type);
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, false);
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    false);

  web_graphics_context->deleteTexture(source_texture);
  web_graphics_context->flush();

  SyncPointClientImpl client(web_graphics_context);
  video_frame->UpdateReleaseSyncPoint(&client);
  return true;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerImpl::generateKeyRequest(const WebString& key_system,
                                       const unsigned char* init_data,
                                       unsigned init_data_length) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return encrypted_media_support_->GenerateKeyRequest(
      frame_, key_system, init_data, init_data_length);
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::addKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return encrypted_media_support_->AddKey(
      key_system, key, key_length, init_data, init_data_length, session_id);
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return encrypted_media_support_->CancelKeyRequest(key_system, session_id);
}

void WebMediaPlayerImpl::setContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  encrypted_media_support_->SetContentDecryptionModule(cdm);
}

void WebMediaPlayerImpl::setContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  encrypted_media_support_->SetContentDecryptionModule(cdm, result);
}

void WebMediaPlayerImpl::setContentDecryptionModuleSync(
    blink::WebContentDecryptionModule* cdm) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  encrypted_media_support_->SetContentDecryptionModuleSync(cdm);
}

void WebMediaPlayerImpl::OnPipelineSeeked(bool time_changed,
                                          PipelineStatus status) {
  DVLOG(1) << __FUNCTION__ << "(" << time_changed << ", " << status << ")";
  DCHECK(main_loop_->BelongsToCurrentThread());
  seeking_ = false;
  if (pending_seek_) {
    pending_seek_ = false;
    seek(pending_seek_seconds_);
    return;
  }

  if (status != media::PIPELINE_OK) {
    OnPipelineError(status);
    return;
  }

  // Update our paused time.
  if (paused_)
    paused_time_ = pipeline_.GetMediaTime();

  should_notify_time_changed_ = time_changed;
}

void WebMediaPlayerImpl::OnPipelineEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_loop_->BelongsToCurrentThread());
  client_->timeChanged();
}

void WebMediaPlayerImpl::OnPipelineError(PipelineStatus error) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DCHECK_NE(error, media::PIPELINE_OK);

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
    return;
  }

  SetNetworkState(PipelineErrorToNetworkState(error));

  if (error == media::PIPELINE_ERROR_DECRYPT)
    encrypted_media_support_->OnPipelineDecryptError();
}

void WebMediaPlayerImpl::OnPipelineMetadata(
    media::PipelineMetadata metadata) {
  DVLOG(1) << __FUNCTION__;

  pipeline_metadata_ = metadata;

  UMA_HISTOGRAM_ENUMERATION("Media.VideoRotation",
                            metadata.video_rotation,
                            media::VIDEO_ROTATION_MAX + 1);
  SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);

  if (hasVideo()) {
    DCHECK(!video_weblayer_);
    scoped_refptr<cc::VideoLayer> layer =
        cc::VideoLayer::Create(compositor_, pipeline_metadata_.video_rotation);

    if (pipeline_metadata_.video_rotation == media::VIDEO_ROTATION_90 ||
        pipeline_metadata_.video_rotation == media::VIDEO_ROTATION_270) {
      gfx::Size size = pipeline_metadata_.natural_size;
      pipeline_metadata_.natural_size = gfx::Size(size.height(), size.width());
    }

    video_weblayer_.reset(new cc_blink::WebLayerImpl(layer));
    video_weblayer_->setOpaque(opaque_);
    client_->setWebLayer(video_weblayer_.get());
  }
}

void WebMediaPlayerImpl::OnPipelineBufferingStateChanged(
    media::BufferingState buffering_state) {
  DVLOG(1) << __FUNCTION__ << "(" << buffering_state << ")";

  // Ignore buffering state changes until we've completed all outstanding seeks.
  if (seeking_ || pending_seek_)
    return;

  // TODO(scherkus): Handle other buffering states when Pipeline starts using
  // them and translate them ready state changes http://crbug.com/144683
  DCHECK_EQ(buffering_state, media::BUFFERING_HAVE_ENOUGH);
  SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  // Blink expects a timeChanged() in response to a seek().
  if (should_notify_time_changed_)
    client_->timeChanged();
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  client_->mediaSourceOpened(new WebMediaSourceImpl(
      chunk_demuxer_, base::Bind(&LogMediaSourceError, media_log_)));
}

void WebMediaPlayerImpl::OnAddTextTrack(
    const media::TextTrackConfig& config,
    const media::AddTextTrackDoneCB& done_cb) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  const WebInbandTextTrackImpl::Kind web_kind =
      static_cast<WebInbandTextTrackImpl::Kind>(config.kind());
  const blink::WebString web_label =
      blink::WebString::fromUTF8(config.label());
  const blink::WebString web_language =
      blink::WebString::fromUTF8(config.language());
  const blink::WebString web_id =
      blink::WebString::fromUTF8(config.id());

  scoped_ptr<WebInbandTextTrackImpl> web_inband_text_track(
      new WebInbandTextTrackImpl(web_kind, web_label, web_language, web_id,
                                 text_track_index_++));

  scoped_ptr<media::TextTrack> text_track(
      new TextTrackImpl(main_loop_, client_, web_inband_text_track.Pass()));

  done_cb.Run(text_track.Pass());
}

void WebMediaPlayerImpl::DataSourceInitialized(bool success) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (!success) {
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
    return;
  }

  StartPipeline();
}

void WebMediaPlayerImpl::NotifyDownloading(bool is_downloading) {
  if (!is_downloading && network_state_ == WebMediaPlayer::NetworkStateLoading)
    SetNetworkState(WebMediaPlayer::NetworkStateIdle);
  else if (is_downloading && network_state_ == WebMediaPlayer::NetworkStateIdle)
    SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  media_log_->AddEvent(
      media_log_->CreateBooleanEvent(
          media::MediaLogEvent::NETWORK_ACTIVITY_SET,
          "is_downloading_data", is_downloading));
}

// TODO(xhwang): Move this to a factory class so that we can create different
// renderers.
scoped_ptr<media::Renderer> WebMediaPlayerImpl::CreateRenderer() {
  media::SetDecryptorReadyCB set_decryptor_ready_cb =
      encrypted_media_support_->CreateSetDecryptorReadyCB();

  // Create our audio decoders and renderer.
  ScopedVector<media::AudioDecoder> audio_decoders;

  media::LogCB log_cb = base::Bind(&LogMediaSourceError, media_log_);
  audio_decoders.push_back(new media::FFmpegAudioDecoder(media_loop_, log_cb));
  audio_decoders.push_back(new media::OpusAudioDecoder(media_loop_));

  scoped_ptr<media::AudioRenderer> audio_renderer(new media::AudioRendererImpl(
      media_loop_,
      audio_source_provider_.get(),
      audio_decoders.Pass(),
      set_decryptor_ready_cb,
      RenderThreadImpl::current()->GetAudioHardwareConfig()));

  // Create our video decoders and renderer.
  ScopedVector<media::VideoDecoder> video_decoders;

  if (gpu_factories_.get()) {
    video_decoders.push_back(
        new media::GpuVideoDecoder(gpu_factories_, media_log_));
  }

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(new media::VpxVideoDecoder(media_loop_));
#endif  // !defined(MEDIA_DISABLE_LIBVPX)

  video_decoders.push_back(new media::FFmpegVideoDecoder(media_loop_));

  scoped_ptr<media::VideoRenderer> video_renderer(
      new media::VideoRendererImpl(
          media_loop_,
          video_decoders.Pass(),
          set_decryptor_ready_cb,
          base::Bind(&WebMediaPlayerImpl::FrameReady, base::Unretained(this)),
          true));

  // Create renderer.
  return scoped_ptr<media::Renderer>(new media::RendererImpl(
      media_loop_,
      demuxer_.get(),
      audio_renderer.Pass(),
      video_renderer.Pass()));
}

void WebMediaPlayerImpl::StartPipeline() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  // Keep track if this is a MSE or non-MSE playback.
  UMA_HISTOGRAM_BOOLEAN("Media.MSE.Playback",
                        (load_type_ == LoadTypeMediaSource));

  media::LogCB mse_log_cb;
  media::Demuxer::NeedKeyCB need_key_cb =
      encrypted_media_support_->CreateNeedKeyCB();

  // Figure out which demuxer to use.
  if (load_type_ != LoadTypeMediaSource) {
    DCHECK(!chunk_demuxer_);
    DCHECK(data_source_);

    demuxer_.reset(new media::FFmpegDemuxer(
        media_loop_, data_source_.get(),
        need_key_cb,
        media_log_));
  } else {
    DCHECK(!chunk_demuxer_);
    DCHECK(!data_source_);

    mse_log_cb = base::Bind(&LogMediaSourceError, media_log_);

    chunk_demuxer_ = new media::ChunkDemuxer(
        BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDemuxerOpened),
        need_key_cb,
        mse_log_cb,
        true);
    demuxer_.reset(chunk_demuxer_);
  }

  scoped_ptr<media::FilterCollection> filter_collection(
      new media::FilterCollection());
  filter_collection->SetDemuxer(demuxer_.get());
  filter_collection->SetRenderer(CreateRenderer());

  if (cmd_line->HasSwitch(switches::kEnableInbandTextTracks)) {
    scoped_ptr<media::TextRenderer> text_renderer(
        new media::TextRenderer(
            media_loop_,
            BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnAddTextTrack)));

    filter_collection->SetTextRenderer(text_renderer.Pass());
  }

  // ... and we're ready to go!
  seeking_ = true;
  pipeline_.Start(
      filter_collection.Pass(),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineEnded),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineError),
      BIND_TO_RENDER_LOOP1(&WebMediaPlayerImpl::OnPipelineSeeked, false),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineMetadata),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineBufferingStateChanged),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDurationChanged));
}

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DVLOG(1) << __FUNCTION__ << "(" << state << ")";
  DCHECK(main_loop_->BelongsToCurrentThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  client_->networkStateChanged();
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DVLOG(1) << __FUNCTION__ << "(" << state << ")";
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (state == WebMediaPlayer::ReadyStateHaveEnoughData && data_source_ &&
      data_source_->assume_fully_buffered() &&
      network_state_ == WebMediaPlayer::NetworkStateLoading)
    SetNetworkState(WebMediaPlayer::NetworkStateLoaded);

  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  client_->readyStateChanged();
}

blink::WebAudioSourceProvider* WebMediaPlayerImpl::audioSourceProvider() {
  return audio_source_provider_.get();
}

double WebMediaPlayerImpl::GetPipelineDuration() const {
  base::TimeDelta duration = pipeline_.GetMediaDuration();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration == media::kInfiniteDuration())
    return std::numeric_limits<double>::infinity();

  return duration.InSecondsF();
}

void WebMediaPlayerImpl::OnDurationChanged() {
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return;

  client_->durationChanged();
}

void WebMediaPlayerImpl::OnNaturalSizeChanged(gfx::Size size) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::ReadyStateHaveNothing);
  TRACE_EVENT0("media", "WebMediaPlayerImpl::OnNaturalSizeChanged");

  media_log_->AddEvent(
      media_log_->CreateVideoSizeSetEvent(size.width(), size.height()));
  pipeline_metadata_.natural_size = size;

  client_->sizeChanged();
}

void WebMediaPlayerImpl::OnOpacityChanged(bool opaque) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::ReadyStateHaveNothing);

  opaque_ = opaque;
  if (video_weblayer_)
    video_weblayer_->setOpaque(opaque_);
}

void WebMediaPlayerImpl::FrameReady(
    const scoped_refptr<media::VideoFrame>& frame) {
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameCompositor::UpdateCurrentFrame,
                 base::Unretained(compositor_),
                 frame));
}

static void GetCurrentFrameAndSignal(
    VideoFrameCompositor* compositor,
    scoped_refptr<media::VideoFrame>* video_frame_out,
    base::WaitableEvent* event) {
  TRACE_EVENT0("media", "GetCurrentFrameAndSignal");
  *video_frame_out = compositor->GetCurrentFrame();
  event->Signal();
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerImpl::GetCurrentFrameFromCompositor() {
  TRACE_EVENT0("media", "WebMediaPlayerImpl::GetCurrentFrameFromCompositor");
  if (compositor_task_runner_->BelongsToCurrentThread())
    return compositor_->GetCurrentFrame();

  // Use a posted task and waitable event instead of a lock otherwise
  // WebGL/Canvas can see different content than what the compositor is seeing.
  scoped_refptr<media::VideoFrame> video_frame;
  base::WaitableEvent event(false, false);
  compositor_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(&GetCurrentFrameAndSignal,
                                               base::Unretained(compositor_),
                                               &video_frame,
                                               &event));
  event.Wait();
  return video_frame;
}

}  // namespace content
