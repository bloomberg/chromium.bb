// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_ms.h"

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/hash.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "cc/blink/context_provider_web_context.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/layers/video_layer.h"
#include "content/public/renderer/media_stream_audio_renderer.h"
#include "content/public/renderer/media_stream_renderer_factory.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/video_frame_provider.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/blink/webgraphicscontext3d_impl.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_rotation.h"
#include "media/base/video_util.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"

using blink::WebCanvas;
using blink::WebMediaPlayer;
using blink::WebRect;
using blink::WebSize;

namespace content {

namespace {

// This function copies |frame| to a new YV12 media::VideoFrame.
scoped_refptr<media::VideoFrame> CopyFrameToYV12(
    const scoped_refptr<media::VideoFrame>& frame,
    media::SkCanvasVideoRenderer* video_renderer) {
  const scoped_refptr<media::VideoFrame> new_frame =
      media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_YV12,
                                     frame->coded_size(), frame->visible_rect(),
                                     frame->natural_size(), frame->timestamp());

  if (frame->HasTextures()) {
    DCHECK(frame->format() == media::PIXEL_FORMAT_ARGB ||
           frame->format() == media::PIXEL_FORMAT_XRGB ||
           frame->format() == media::PIXEL_FORMAT_I420 ||
           frame->format() == media::PIXEL_FORMAT_UYVY);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(frame->visible_rect().width(),
                          frame->visible_rect().height());
    SkCanvas canvas(bitmap);

    cc::ContextProvider* const provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
    if (provider) {
      const media::Context3D context_3d =
          media::Context3D(provider->ContextGL(), provider->GrContext());
      DCHECK(context_3d.gl);
      video_renderer->Copy(frame.get(), &canvas, context_3d);
    } else {
      // GPU Process crashed.
      bitmap.eraseColor(SK_ColorTRANSPARENT);
    }
    media::CopyRGBToVideoFrame(reinterpret_cast<uint8*>(bitmap.getPixels()),
                               bitmap.rowBytes(),
                               frame->visible_rect(),
                               new_frame.get());
  } else {
    DCHECK(frame->IsMappable());
    DCHECK(frame->format() == media::PIXEL_FORMAT_YV12 ||
           frame->format() == media::PIXEL_FORMAT_I420);
    for (size_t i = 0; i < media::VideoFrame::NumPlanes(frame->format()); ++i) {
      media::CopyPlane(i, frame->data(i), frame->stride(i),
                       frame->rows(i), new_frame.get());
    }
  }
  return new_frame;
}

}  // anonymous namespace

WebMediaPlayerMS::WebMediaPlayerMS(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
    media::MediaLog* media_log,
    scoped_ptr<MediaStreamRendererFactory> factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories)
    : frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      buffered_(static_cast<size_t>(0)),
      volume_(1.0f),
      client_(client),
      delegate_(delegate),
      paused_(true),
      received_first_frame_(false),
      media_log_(media_log),
      renderer_factory_(factory.Pass()),
      media_task_runner_(media_task_runner),
      worker_task_runner_(worker_task_runner),
      gpu_factories_(gpu_factories),
      compositor_(new Compositor(compositor_task_runner)),
      compositor_task_runner_(compositor_task_runner) {
  DVLOG(1) << "WebMediaPlayerMS::ctor";
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerMS::~WebMediaPlayerMS() {
  DVLOG(1) << "WebMediaPlayerMS::dtor";
  DCHECK(thread_checker_.CalledOnValidThread());

  compositor_task_runner_->DeleteSoon(FROM_HERE, compositor_.release());

  GetClient()->setWebLayer(NULL);

  if (video_frame_provider_.get())
    video_frame_provider_->Stop();

  if (audio_renderer_.get())
    audio_renderer_->Stop();

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (delegate_.get())
    delegate_->PlayerGone(this);
}

void WebMediaPlayerMS::load(LoadType load_type,
                            const blink::WebURL& url,
                            CORSMode cors_mode) {
  DVLOG(1) << "WebMediaPlayerMS::load";
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(acolwell): Change this to DCHECK_EQ(load_type,
  // LoadTypeMediaStream) once Blink-side changes land.
  DCHECK_NE(load_type, LoadTypeMediaSource);

  blink::WebMediaStream web_stream(
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  web_stream.videoTracks(tracks);

  bool remote = tracks.size() && tracks[0].source().remote();

  bool algorithm_enabled = remote &&
                           base::CommandLine::ForCurrentProcess()->HasSwitch(
                               switches::kEnableRTCSmoothnessAlgorithm);

  compositor_->SetAlgorithmEnabled(algorithm_enabled);

  uint32 hash_value = base::Hash(url.string().utf8());
  compositor_->SetSerial(((hash_value & 0x7FFFFFFF) << 1) | (remote ? 1 : 0));

  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  video_frame_provider_ = renderer_factory_->GetVideoFrameProvider(
      url,
      base::Bind(&WebMediaPlayerMS::OnSourceError, AsWeakPtr()),
      base::Bind(&WebMediaPlayerMS::OnFrameAvailable, AsWeakPtr()),
      media_task_runner_,
      worker_task_runner_,
      gpu_factories_);

  RenderFrame* const frame = RenderFrame::FromWebFrame(frame_);
  audio_renderer_ = renderer_factory_->GetAudioRenderer(
      url, frame->GetRoutingID(), std::string(), url::Origin());

  if (!video_frame_provider_ && !audio_renderer_) {
    SetNetworkState(WebMediaPlayer::NetworkStateNetworkError);
    return;
  }

  if (audio_renderer_) {
    audio_renderer_->SetVolume(volume_);
    audio_renderer_->Start();
  }
  if (video_frame_provider_)
    video_frame_provider_->Start();
  if (audio_renderer_ && !video_frame_provider_) {
    // This is audio-only mode.
    SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
  }
}

void WebMediaPlayerMS::play() {
  DVLOG(1) << "WebMediaPlayerMS::play";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (paused_) {
    if (video_frame_provider_.get())
      video_frame_provider_->Play();

    compositor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WebMediaPlayerMS::Compositor::StartRendering,
                              base::Unretained(compositor_.get())));

    if (audio_renderer_.get())
      audio_renderer_->Play();

    if (delegate_.get())
      delegate_->DidPlay(this);
  }

  paused_ = false;

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));
}

void WebMediaPlayerMS::pause() {
  DVLOG(1) << "WebMediaPlayerMS::pause";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (video_frame_provider_.get())
    video_frame_provider_->Pause();

  compositor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebMediaPlayerMS::Compositor::StopRendering,
                            base::Unretained(compositor_.get())));
  compositor_->ReplaceCurrentFrameWithACopy(&video_renderer_);

  if (!paused_) {
    if (audio_renderer_.get())
      audio_renderer_->Pause();

    if (delegate_.get())
      delegate_->DidPause(this);
  }

  paused_ = true;

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));
}

bool WebMediaPlayerMS::supportsSave() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

void WebMediaPlayerMS::seek(double seconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setRate(double rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setVolume(double volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::setVolume(volume=" << volume << ")";
  volume_ = volume;
  if (audio_renderer_.get())
    audio_renderer_->SetVolume(volume_);
}

void WebMediaPlayerMS::setSinkId(const blink::WebString& sink_id,
                                 media::WebSetSinkIdCB* web_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << __FUNCTION__;
  media::SwitchOutputDeviceCB callback =
      media::ConvertToSwitchOutputDeviceCB(web_callback);
  if (audio_renderer_.get()) {
    media::OutputDevice* output_device = audio_renderer_->GetOutputDevice();
    if (output_device) {
      output_device->SwitchOutputDevice(sink_id.utf8(),
                                        frame_->securityOrigin(), callback);
      return;
    }
  }
  callback.Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
}

void WebMediaPlayerMS::setPreload(WebMediaPlayer::Preload preload) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool WebMediaPlayerMS::hasVideo() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (video_frame_provider_.get() != NULL);
}

bool WebMediaPlayerMS::hasAudio() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (audio_renderer_.get() != NULL);
}

blink::WebSize WebMediaPlayerMS::naturalSize() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  gfx::Size size = compositor_->GetCurrentSize();

  DVLOG(3) << "WebMediaPlayerMS::naturalSize, " << size.ToString();
  return blink::WebSize(size);
}

bool WebMediaPlayerMS::paused() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return paused_;
}

bool WebMediaPlayerMS::seeking() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

double WebMediaPlayerMS::duration() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::numeric_limits<double>::infinity();
}

double WebMediaPlayerMS::currentTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::TimeDelta current_time = compositor_->GetCurrentTime();
  if (current_time.ToInternalValue() != 0) {
    return current_time.InSecondsF();
  } else if (audio_renderer_.get()) {
    return audio_renderer_->GetCurrentRenderTime().InSecondsF();
  }
  return 0.0;
}

WebMediaPlayer::NetworkState WebMediaPlayerMS::networkState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::networkState, state:" << network_state_;
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerMS::readyState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::readyState, state:" << ready_state_;
  return ready_state_;
}

blink::WebTimeRanges WebMediaPlayerMS::buffered() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return buffered_;
}

blink::WebTimeRanges WebMediaPlayerMS::seekable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return blink::WebTimeRanges();
}

bool WebMediaPlayerMS::didLoadingProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void WebMediaPlayerMS::paint(blink::WebCanvas* canvas,
                             const blink::WebRect& rect,
                             unsigned char alpha,
                             SkXfermode::Mode mode) {
  DVLOG(3) << "WebMediaPlayerMS::paint";
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<media::VideoFrame> frame = compositor_->GetCurrentFrame();

  media::Context3D context_3d;
  if (frame.get() && frame->HasTextures()) {
    cc::ContextProvider* provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
    // GPU Process crashed.
    if (!provider)
      return;
    context_3d = media::Context3D(provider->ContextGL(), provider->GrContext());
    DCHECK(context_3d.gl);
  }
  gfx::RectF dest_rect(rect.x, rect.y, rect.width, rect.height);
  video_renderer_.Paint(frame, canvas, dest_rect, alpha, mode,
                                         media::VIDEO_ROTATION_0, context_3d);
}

bool WebMediaPlayerMS::hasSingleSecurityOrigin() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

bool WebMediaPlayerMS::didPassCORSAccessCheck() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

double WebMediaPlayerMS::mediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerMS::decodedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  unsigned total_frame_count = compositor_->GetTotalFrameCount();
  DVLOG(1) << "WebMediaPlayerMS::decodedFrameCount, " << total_frame_count;
  return total_frame_count;
}

unsigned WebMediaPlayerMS::droppedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  unsigned dropped_frame_count = compositor_->GetDroppedFrameCount();
  DVLOG(1) << "WebMediaPlayerMS::droppedFrameCount, " << dropped_frame_count;
  return dropped_frame_count;
}

unsigned WebMediaPlayerMS::audioDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerMS::videoDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

bool WebMediaPlayerMS::copyVideoTextureToPlatformTexture(
    blink::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  TRACE_EVENT0("media", "WebMediaPlayerMS:copyVideoTextureToPlatformTexture");
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<media::VideoFrame> video_frame = compositor_->GetCurrentFrame();

  if (!video_frame.get() || video_frame->HasTextures() ||
      media::VideoFrame::NumPlanes(video_frame->format()) != 1) {
    return false;
  }

  // TODO(dshwang): need more elegant way to convert WebGraphicsContext3D to
  // GLES2Interface.
  gpu::gles2::GLES2Interface* const gl =
      static_cast<gpu_blink::WebGraphicsContext3DImpl*>(web_graphics_context)
          ->GetGLInterface();
  media::SkCanvasVideoRenderer::CopyVideoFrameSingleTextureToGLTexture(
      gl, video_frame.get(), texture, internal_format, type, premultiply_alpha,
      flip_y);
  return true;
}

void WebMediaPlayerMS::OnFrameAvailable(
    const scoped_refptr<media::VideoFrame>& frame) {
  DVLOG(3) << "WebMediaPlayerMS::OnFrameAvailable";
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeTicks render_time;
  if (!frame->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
    DCHECK(!compositor_->GetAlgorithmEnabled() || !received_first_frame_);
    render_time = base::TimeTicks();
  }
  TRACE_EVENT1("webrtc", "WebMediaPlayerMS::OnFrameAvailable",
               "Ideal Render Instant", render_time.ToInternalValue());

  if (!received_first_frame_) {
    received_first_frame_ = true;
    SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

    if (video_frame_provider_.get()) {
      video_weblayer_.reset(new cc_blink::WebLayerImpl(
          cc::VideoLayer::Create(cc_blink::WebLayerImpl::LayerSettings(),
                                 compositor_.get(), media::VIDEO_ROTATION_0)));
      video_weblayer_->layer()->SetContentsOpaque(true);
      video_weblayer_->SetContentsOpaqueIsFixed(true);
      GetClient()->setWebLayer(video_weblayer_.get());
    }

    // REFERENCE_TIME isn't prepared for this stream. VideoRendererAlgorithm
    // cannot work in this case, force it off.
    if (render_time.is_null() && compositor_->GetAlgorithmEnabled()) {
      LOG(DFATAL) << "REFERENCE_TIME is not prepared, so VideoRendererAlgorithm"
                     " has been turned off.";
      compositor_->SetAlgorithmEnabled(false);
    }
  }

  bool size_changed = compositor_->GetCurrentSize() != frame->natural_size();

  compositor_->EnqueueFrame(frame);

  if (size_changed)
    GetClient()->sizeChanged();
}

void WebMediaPlayerMS::RepaintInternal() {
  DVLOG(1) << "WebMediaPlayerMS::RepaintInternal";
  DCHECK(thread_checker_.CalledOnValidThread());
  GetClient()->repaint();
}

void WebMediaPlayerMS::OnSourceError() {
  DVLOG(1) << "WebMediaPlayerMS::OnSourceError";
  DCHECK(thread_checker_.CalledOnValidThread());
  SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
  RepaintInternal();
}

void WebMediaPlayerMS::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->networkStateChanged();
}

void WebMediaPlayerMS::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->readyStateChanged();
}

blink::WebMediaPlayerClient* WebMediaPlayerMS::GetClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client_);
  return client_;
}

WebMediaPlayerMS::Compositor::Compositor(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner)
    : compositor_task_runner_(compositor_task_runner),
      video_frame_provider_client_(NULL),
      frame_pool_(nullptr),
      current_frame_used_(false),
      last_deadline_max_(base::TimeTicks()),
      last_render_length_(base::TimeDelta::FromSecondsD(1.0 / 60.0)),
      total_frame_count_(0),
      dropped_frame_count_(0),
      paused_(true) {}

WebMediaPlayerMS::Compositor::~Compositor() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
}

bool WebMediaPlayerMS::Compositor::GetWallClockTimes(
    const std::vector<base::TimeDelta>& timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  auto iter = timestamps_to_clock_times_.begin();
  auto end = timestamps_to_clock_times_.end();
  for (const base::TimeDelta& timestamp : timestamps) {
    while (iter != end && iter->first < timestamp)
      ++iter;
    DCHECK(iter != end && iter->first == timestamp);
    wall_clock_times->push_back(iter->second);
  }

  return true;
}

void WebMediaPlayerMS::Compositor::Render(base::TimeTicks deadline_min,
                                          base::TimeTicks deadline_max) {
  last_deadline_max_ = deadline_max;
  last_render_length_ = deadline_max - deadline_min;

  size_t frames_dropped = 0;
  scoped_refptr<media::VideoFrame> frame =
      frame_pool_->Render(deadline_min, deadline_max, &frames_dropped);
  dropped_frame_count_ += frames_dropped;

  if (frame == current_frame_)
    return;

  SetCurrentFrame(frame);

  while (!timestamps_to_clock_times_.empty() &&
         timestamps_to_clock_times_.front().first < frame->timestamp())
    timestamps_to_clock_times_.pop_front();
}

void WebMediaPlayerMS::Compositor::EnqueueFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(current_frame_lock_);
  ++total_frame_count_;

  if (!frame_pool_) {
    SetCurrentFrame(frame);
    return;
  }

  // This is a signal frame saying that the stream is stopped.
  if (current_frame_ && frame->timestamp().is_zero())
    return;

  base::TimeTicks render_time;
  const base::TimeTicks now = base::TimeTicks::Now();
  CHECK(frame->metadata()->GetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, &render_time));
  DCHECK(timestamps_to_clock_times_.empty() ||
         frame->timestamp() > timestamps_to_clock_times_.rbegin()->first);
  timestamps_to_clock_times_.push_back(
      std::make_pair(frame->timestamp(), render_time));

  frame_pool_->EnqueueFrame(frame);

  if (now > last_deadline_max_) {
    // This shows vsyncs stops rendering frames. A probable cause is that the
    // tab is not in the front. But we still have to let old frames go.
    base::TimeTicks deadline_max = last_deadline_max_ + last_render_length_;
    if (deadline_max < now)
      deadline_max = now;

    Render(deadline_max - last_render_length_, deadline_max);
  }
}

bool WebMediaPlayerMS::Compositor::UpdateCurrentFrame(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(current_frame_lock_);
  TRACE_EVENT_BEGIN2("webrtc", "WebMediaPlayerMS::UpdateCurrentFrame",
                     "Actual Render Begin", deadline_min.ToInternalValue(),
                     "Actual Render End", deadline_max.ToInternalValue());
  if (paused_)
    return false;

  if (frame_pool_)
    Render(deadline_min, deadline_max);

  base::TimeTicks render_time;
  if (!current_frame_->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
    DCHECK(!frame_pool_);
    render_time = base::TimeTicks();
  }

  TRACE_EVENT_END2("webrtc", "WebMediaPlayerMS::UpdateCurrentFrame",
                   "Ideal Render Instant", render_time.ToInternalValue(),
                   "Serial", serial_);
  return !current_frame_used_;
}

bool WebMediaPlayerMS::Compositor::HasCurrentFrame() {
  base::AutoLock auto_lock(current_frame_lock_);
  return !!current_frame_.get();
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerMS::Compositor::GetCurrentFrame() {
  DVLOG(3) << "WebMediaPlayerMS::Compositor::GetCurrentFrame";
  base::AutoLock auto_lock(current_frame_lock_);
  if (!current_frame_.get())
    return NULL;
  return current_frame_;
}

void WebMediaPlayerMS::Compositor::PutCurrentFrame() {
  DVLOG(3) << "WebMediaPlayerMS::PutCurrentFrame";
  current_frame_used_ = true;
}

void WebMediaPlayerMS::Compositor::SetCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (!current_frame_used_)
    ++dropped_frame_count_;
  current_frame_used_ = false;
  current_frame_ = frame;
}

void WebMediaPlayerMS::Compositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();

  video_frame_provider_client_ = client;
  if (video_frame_provider_client_ && !paused_)
    video_frame_provider_client_->StartRendering();
}

void WebMediaPlayerMS::Compositor::StartRendering() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  paused_ = false;

  // It is possible that the video gets paused and then resumed. We need to
  // reset VideoRendererAlgorithm, otherwise, VideoRendererAlgorithm will think
  // there is a very long frame in the queue and then make totally wrong
  // frame selection.
  if (frame_pool_) {
    base::AutoLock auto_lock(current_frame_lock_);
    frame_pool_->Reset();
  }

  if (video_frame_provider_client_)
    video_frame_provider_client_->StartRendering();
}

void WebMediaPlayerMS::Compositor::StopRendering() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  paused_ = true;
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopRendering();
}

void WebMediaPlayerMS::Compositor::ReplaceCurrentFrameWithACopy(
    media::SkCanvasVideoRenderer* renderer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(current_frame_lock_);
  if (!current_frame_.get())
    return;

  // Copy the frame so that rendering can show the last received frame.
  // The original frame must not be referenced when the player is paused since
  // there might be a finite number of available buffers. E.g, video that
  // originates from a video camera.
  scoped_refptr<media::VideoFrame> new_frame =
      CopyFrameToYV12(current_frame_, renderer);

  current_frame_ = new_frame;
}

gfx::Size WebMediaPlayerMS::Compositor::GetCurrentSize() {
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_.get() ? current_frame_->natural_size() : gfx::Size();
}

base::TimeDelta WebMediaPlayerMS::Compositor::GetCurrentTime() {
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_.get() ? current_frame_->timestamp() : base::TimeDelta();
}

unsigned WebMediaPlayerMS::Compositor::GetTotalFrameCount() {
  return total_frame_count_;
}

unsigned WebMediaPlayerMS::Compositor::GetDroppedFrameCount() {
  return dropped_frame_count_;
}

void WebMediaPlayerMS::Compositor::SetAlgorithmEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enabled)
    frame_pool_.reset(new media::VideoRendererAlgorithm(
        base::Bind(&WebMediaPlayerMS::Compositor::GetWallClockTimes,
                   base::Unretained(this))));
  else
    frame_pool_.reset();
}

bool WebMediaPlayerMS::Compositor::GetAlgorithmEnabled() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return !!frame_pool_;
}

void WebMediaPlayerMS::Compositor::SetSerial(uint32 serial) {
  DCHECK(thread_checker_.CalledOnValidThread());
  serial_ = serial;
}
}  // namespace content
