// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_ms.h"

#include <stddef.h>
#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "cc/blink/context_provider_web_context.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/layers/video_layer.h"
#include "content/public/renderer/media_stream_audio_renderer.h"
#include "content/public/renderer/media_stream_renderer_factory.h"
#include "content/public/renderer/video_frame_provider.h"
#include "content/renderer/media/webmediaplayer_ms_compositor.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/blink/webgraphicscontext3d_impl.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

WebMediaPlayerMS::WebMediaPlayerMS(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
    media::MediaLog* media_log,
    scoped_ptr<MediaStreamRendererFactory> factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin)
    : frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      buffered_(static_cast<size_t>(0)),
      client_(client),
      delegate_(delegate),
      delegate_id_(0),
      paused_(true),
      render_frame_suspended_(false),
      received_first_frame_(false),
      media_log_(media_log),
      renderer_factory_(std::move(factory)),
      media_task_runner_(media_task_runner),
      worker_task_runner_(worker_task_runner),
      gpu_factories_(gpu_factories),
      compositor_task_runner_(compositor_task_runner),
      initial_audio_output_device_id_(sink_id.utf8()),
      initial_security_origin_(security_origin.isNull()
                                   ? url::Origin()
                                   : url::Origin(security_origin)),
      volume_(1.0),
      volume_multiplier_(1.0) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(client);
  if (delegate_)
    delegate_id_ = delegate_->AddObserver(this);

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerMS::~WebMediaPlayerMS() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (compositor_ && !compositor_task_runner_->BelongsToCurrentThread())
    compositor_task_runner_->DeleteSoon(FROM_HERE, compositor_.release());

  get_client()->setWebLayer(nullptr);

  if (video_frame_provider_)
    video_frame_provider_->Stop();

  if (audio_renderer_)
    audio_renderer_->Stop();

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (delegate_) {
    delegate_->PlayerGone(delegate_id_);
    delegate_->RemoveObserver(delegate_id_);
  }
}

void WebMediaPlayerMS::load(LoadType load_type,
                            const blink::WebURL& url,
                            CORSMode /*cors_mode*/) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(acolwell): Change this to DCHECK_EQ(load_type, LoadTypeMediaStream)
  // once Blink-side changes land.
  DCHECK_NE(load_type, LoadTypeMediaSource);

  compositor_.reset(new WebMediaPlayerMSCompositor(compositor_task_runner_, url,
                                                   AsWeakPtr()));

  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.string().utf8()));

  video_frame_provider_ = renderer_factory_->GetVideoFrameProvider(
      url,
      base::Bind(&WebMediaPlayerMS::OnSourceError, AsWeakPtr()),
      base::Bind(&WebMediaPlayerMS::OnFrameAvailable, AsWeakPtr()),
      media_task_runner_,
      worker_task_runner_,
      gpu_factories_);

  RenderFrame* const frame = RenderFrame::FromWebFrame(frame_);

  if (frame) {
    audio_renderer_ = renderer_factory_->GetAudioRenderer(
        url, frame->GetRoutingID(), initial_audio_output_device_id_,
        initial_security_origin_);
  }

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
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (paused_) {
    if (video_frame_provider_.get())
      video_frame_provider_->Play();

    compositor_->StartRendering();

    if (audio_renderer_.get())
      audio_renderer_->Play();

    if (delegate_) {
      delegate_->DidPlay(delegate_id_, hasVideo(), hasAudio(), false,
                         media::kInfiniteDuration());
    }
  }

  paused_ = false;

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));
}

void WebMediaPlayerMS::pause() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (video_frame_provider_.get())
    video_frame_provider_->Pause();

  compositor_->StopRendering();
  compositor_->ReplaceCurrentFrameWithACopy();

  if (!paused_) {
    if (audio_renderer_.get())
      audio_renderer_->Pause();

    if (delegate_)
      delegate_->DidPause(delegate_id_, false);
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
  DVLOG(1) << __FUNCTION__ << "(volume=" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  volume_ = volume;
  if (audio_renderer_.get())
    audio_renderer_->SetVolume(volume_ * volume_multiplier_);
}

void WebMediaPlayerMS::setSinkId(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  const media::SwitchOutputDeviceCB callback =
      media::ConvertToSwitchOutputDeviceCB(web_callback);
  if (audio_renderer_ && audio_renderer_->GetOutputDevice()) {
    audio_renderer_->GetOutputDevice()->SwitchOutputDevice(
        sink_id.utf8(), security_origin, callback);
  } else {
    callback.Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  }
}

void WebMediaPlayerMS::setPreload(WebMediaPlayer::Preload preload) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool WebMediaPlayerMS::hasVideo() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (video_frame_provider_.get() != nullptr);
}

bool WebMediaPlayerMS::hasAudio() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (audio_renderer_.get() != nullptr);
}

blink::WebSize WebMediaPlayerMS::naturalSize() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return blink::WebSize(compositor_->GetCurrentSize());
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
  const base::TimeDelta current_time = compositor_->GetCurrentTime();
  if (current_time.ToInternalValue() != 0)
    return current_time.InSecondsF();
  else if (audio_renderer_.get())
    return audio_renderer_->GetCurrentRenderTime().InSecondsF();
  return 0.0;
}

blink::WebMediaPlayer::NetworkState WebMediaPlayerMS::networkState() const {
  DVLOG(1) << __FUNCTION__ << ", state:" << network_state_;
  DCHECK(thread_checker_.CalledOnValidThread());
  return network_state_;
}

blink::WebMediaPlayer::ReadyState WebMediaPlayerMS::readyState() const {
  DVLOG(1) << __FUNCTION__ << ", state:" << ready_state_;
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  const scoped_refptr<media::VideoFrame> frame = compositor_->GetCurrentFrame();

  media::Context3D context_3d;
  if (frame && frame->HasTextures()) {
    cc::ContextProvider* provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
    // GPU Process crashed.
    if (!provider)
      return;
    context_3d = media::Context3D(provider->ContextGL(), provider->GrContext());
    DCHECK(context_3d.gl);
  }
  const gfx::RectF dest_rect(rect.x, rect.y, rect.width, rect.height);
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
  return compositor_->total_frame_count();
}

unsigned WebMediaPlayerMS::droppedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return compositor_->dropped_frame_count();
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

void WebMediaPlayerMS::OnHidden() {
#if defined(OS_ANDROID)
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!render_frame_suspended_);

  // Method called when the RenderFrame is sent to background and suspended
  // (android). Substitute the displayed VideoFrame with a copy to avoid
  // holding on to it unnecessarily.
  render_frame_suspended_ = true;
  if (!paused_)
    compositor_->ReplaceCurrentFrameWithACopy();
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnShown() {
#if defined(OS_ANDROID)
  DCHECK(thread_checker_.CalledOnValidThread());

  render_frame_suspended_ = false;
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnPlay() {
  play();
  client_->playbackStateChanged();
}

void WebMediaPlayerMS::OnPause() {
  pause();
  client_->playbackStateChanged();
}

void WebMediaPlayerMS::OnVolumeMultiplierUpdate(double multiplier) {
  volume_multiplier_ = multiplier;
  setVolume(volume_);
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
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (render_frame_suspended_)
    return;

  base::TimeTicks render_time;
  if (frame->metadata()->GetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                                      &render_time)) {
    TRACE_EVENT1("webrtc", "WebMediaPlayerMS::OnFrameAvailable",
                 "Ideal Render Instant", render_time.ToInternalValue());
  } else {
    TRACE_EVENT0("webrtc", "WebMediaPlayerMS::OnFrameAvailable");
  }

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
      get_client()->setWebLayer(video_weblayer_.get());
    }
  }

  // As EnqueueFrame can potentially change |current_frame_|, we need to do
  // the size change check before it. Otherwise, we are running the risk of not
  // detecting a size change event.
  const bool size_changed =
      compositor_->GetCurrentSize() != frame->natural_size();

  compositor_->EnqueueFrame(frame);
  if (size_changed)
    get_client()->sizeChanged();
}

void WebMediaPlayerMS::RepaintInternal() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  get_client()->repaint();
}

void WebMediaPlayerMS::OnSourceError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
  RepaintInternal();
}

void WebMediaPlayerMS::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  get_client()->networkStateChanged();
}

void WebMediaPlayerMS::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  get_client()->readyStateChanged();
}

media::SkCanvasVideoRenderer* WebMediaPlayerMS::GetSkCanvasVideoRenderer() {
  return &video_renderer_;
}

void WebMediaPlayerMS::ResetCanvasCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_renderer_.ResetCache();
}

}  // namespace content
