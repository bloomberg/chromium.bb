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
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/layers/video_layer.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/renderer/media_stream_audio_renderer.h"
#include "content/public/renderer/media_stream_renderer_factory.h"
#include "content/public/renderer/media_stream_video_renderer.h"
#include "content/renderer/media/web_media_element_source_utils.h"
#include "content/renderer/media/webmediaplayer_ms_compositor.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/base/video_rotation.h"
#include "media/base/video_types.h"
#include "media/blink/webmediaplayer_util.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerSource.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"

namespace content {

// FrameDeliverer is responsible for delivering frames received on
// compositor thread by calling of EnqueueFrame() method of |compositor_|.
//
// It is created on the main thread, but methods should be called and class
// should be destructed on the compositor thread.
class WebMediaPlayerMS::FrameDeliverer {
 public:
  typedef base::Callback<void(scoped_refptr<media::VideoFrame>)>
      EnqueueFrameCallback;

  FrameDeliverer(const base::WeakPtr<WebMediaPlayerMS>& player,
                 const EnqueueFrameCallback& enqueue_frame_cb)
      : last_frame_opaque_(true),
        last_frame_rotation_(media::VIDEO_ROTATION_0),
        received_first_frame_(false),
        main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        player_(player),
        enqueue_frame_cb_(enqueue_frame_cb),
        weak_factory_for_compositor_(this) {
    compositor_thread_checker_.DetachFromThread();
  }

  ~FrameDeliverer() {
    DCHECK(compositor_thread_checker_.CalledOnValidThread());
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame) {
    DCHECK(compositor_thread_checker_.CalledOnValidThread());

#if defined(OS_ANDROID)
    if (render_frame_suspended_)
      return;
#endif  // defined(OS_ANDROID)

    base::TimeTicks render_time;
    if (frame->metadata()->GetTimeTicks(
            media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
      TRACE_EVENT1("webrtc", "WebMediaPlayerMS::OnFrameAvailable",
                   "Ideal Render Instant", render_time.ToInternalValue());
    } else {
      TRACE_EVENT0("webrtc", "WebMediaPlayerMS::OnFrameAvailable");
    }

    const bool is_opaque = media::IsOpaque(frame->format());
    media::VideoRotation video_rotation = media::VIDEO_ROTATION_0;
    ignore_result(frame->metadata()->GetRotation(
        media::VideoFrameMetadata::ROTATION, &video_rotation));

    if (!received_first_frame_) {
      received_first_frame_ = true;
      last_frame_opaque_ = is_opaque;
      last_frame_rotation_ = video_rotation;
      main_task_runner_->PostTask(
          FROM_HERE, base::Bind(&WebMediaPlayerMS::OnFirstFrameReceived,
                                player_, video_rotation, is_opaque));
    } else {
      if (last_frame_opaque_ != is_opaque) {
        last_frame_opaque_ = is_opaque;
        main_task_runner_->PostTask(
            FROM_HERE, base::Bind(&WebMediaPlayerMS::OnOpacityChanged, player_,
                                  is_opaque));
      }
      if (last_frame_rotation_ != video_rotation) {
        last_frame_rotation_ = video_rotation;
        main_task_runner_->PostTask(
            FROM_HERE, base::Bind(&WebMediaPlayerMS::OnRotationChanged, player_,
                                  video_rotation, is_opaque));
      }
    }

    enqueue_frame_cb_.Run(frame);
  }

#if defined(OS_ANDROID)
  void SetRenderFrameSuspended(bool render_frame_suspended) {
    DCHECK(compositor_thread_checker_.CalledOnValidThread());
    render_frame_suspended_ = render_frame_suspended;
  }
#endif  // defined(OS_ANDROID)

  MediaStreamVideoRenderer::RepaintCB GetRepaintCallback() {
    return base::Bind(&FrameDeliverer::OnVideoFrame,
                      weak_factory_for_compositor_.GetWeakPtr());
  }

 private:
  bool last_frame_opaque_;
  media::VideoRotation last_frame_rotation_;
  bool received_first_frame_;

#if defined(OS_ANDROID)
  bool render_frame_suspended_ = false;
#endif  // defined(OS_ANDROID)

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const base::WeakPtr<WebMediaPlayerMS> player_;
  const EnqueueFrameCallback enqueue_frame_cb_;

  // Used for DCHECKs to ensure method calls executed on the correct thread,
  // i.e. compositor thread.
  base::ThreadChecker compositor_thread_checker_;

  base::WeakPtrFactory<FrameDeliverer> weak_factory_for_compositor_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

WebMediaPlayerMS::WebMediaPlayerMS(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
    media::MediaLog* media_log,
    std::unique_ptr<MediaStreamRendererFactory> factory,
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
      video_rotation_(media::VIDEO_ROTATION_0),
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
      volume_multiplier_(1.0),
      should_play_upon_shown_(false) {
  DVLOG(1) << __func__;
  DCHECK(client);
  if (delegate_)
    delegate_id_ = delegate_->AddObserver(this);

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerMS::~WebMediaPlayerMS() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Destruct compositor resources in the proper order.
  get_client()->setWebLayer(nullptr);
  if (video_weblayer_)
    static_cast<cc::VideoLayer*>(video_weblayer_->layer())->StopUsingProvider();

  if (frame_deliverer_)
    compositor_task_runner_->DeleteSoon(FROM_HERE, frame_deliverer_.release());

  if (compositor_)
    compositor_task_runner_->DeleteSoon(FROM_HERE, compositor_.release());

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
                            const blink::WebMediaPlayerSource& source,
                            CORSMode /*cors_mode*/) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(acolwell): Change this to DCHECK_EQ(load_type, LoadTypeMediaStream)
  // once Blink-side changes land.
  DCHECK_NE(load_type, LoadTypeMediaSource);
  blink::WebMediaStream web_stream =
      GetWebMediaStreamFromWebMediaPlayerSource(source);

  compositor_.reset(new WebMediaPlayerMSCompositor(compositor_task_runner_,
                                                   web_stream, AsWeakPtr()));

  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  std::string stream_id =
      web_stream.isNull() ? std::string() : web_stream.id().utf8();
  media_log_->AddEvent(media_log_->CreateLoadEvent(stream_id));

  // base::Unretained usage is safe here because |compositor_| is destroyed
  // after |frame_deliverer_|.
  frame_deliverer_.reset(new WebMediaPlayerMS::FrameDeliverer(
      AsWeakPtr(), base::Bind(&WebMediaPlayerMSCompositor::EnqueueFrame,
                              base::Unretained(compositor_.get()))));
  video_frame_provider_ = renderer_factory_->GetVideoRenderer(
      web_stream, media::BindToCurrentLoop(base::Bind(
                      &WebMediaPlayerMS::OnSourceError, AsWeakPtr())),
      frame_deliverer_->GetRepaintCallback(), compositor_task_runner_,
      media_task_runner_, worker_task_runner_, gpu_factories_);

  RenderFrame* const frame = RenderFrame::FromWebFrame(frame_);

  if (frame) {
    // Report UMA and RAPPOR metrics.
    GURL url = source.isURL() ? GURL(source.getAsURL()) : GURL();
    media::ReportMetrics(load_type, url, frame_->getSecurityOrigin());

    audio_renderer_ = renderer_factory_->GetAudioRenderer(
        web_stream, frame->GetRoutingID(), initial_audio_output_device_id_,
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
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));
  if (!paused_)
    return;

  if (video_frame_provider_)
    video_frame_provider_->Resume();

  compositor_->StartRendering();

  if (audio_renderer_)
    audio_renderer_->Play();

  if (delegate_) {
    // TODO(perkj, magjed): We use OneShot focus type here so that it takes
    // audio focus once it starts, and then will not respond to further audio
    // focus changes. See http://crbug.com/596516 for more details.
    delegate_->DidPlay(delegate_id_, hasVideo(), hasAudio(), false,
                       media::MediaContentType::OneShot);
  }

  paused_ = false;
}

void WebMediaPlayerMS::pause() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  should_play_upon_shown_ = false;
  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));
  if (paused_)
    return;

  if (video_frame_provider_)
    video_frame_provider_->Pause();

  compositor_->StopRendering();
  compositor_->ReplaceCurrentFrameWithACopy();

  if (audio_renderer_)
    audio_renderer_->Pause();

  if (delegate_)
    delegate_->DidPause(delegate_id_, false);

  paused_ = true;
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
  DVLOG(1) << __func__ << "(volume=" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  volume_ = volume;
  if (audio_renderer_.get())
    audio_renderer_->SetVolume(volume_ * volume_multiplier_);
}

void WebMediaPlayerMS::setSinkId(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callback) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  const media::OutputDeviceStatusCB callback =
      media::ConvertToOutputDeviceStatusCB(web_callback);
  if (audio_renderer_) {
    audio_renderer_->SwitchOutputDevice(sink_id.utf8(), security_origin,
                                        callback);
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
  if (video_rotation_ == media::VIDEO_ROTATION_90 ||
      video_rotation_ == media::VideoRotation::VIDEO_ROTATION_270) {
    const gfx::Size& current_size = compositor_->GetCurrentSize();
    return blink::WebSize(current_size.height(), current_size.width());
  }
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

blink::WebMediaPlayer::NetworkState WebMediaPlayerMS::getNetworkState() const {
  DVLOG(1) << __func__ << ", state:" << network_state_;
  DCHECK(thread_checker_.CalledOnValidThread());
  return network_state_;
}

blink::WebMediaPlayer::ReadyState WebMediaPlayerMS::getReadyState() const {
  DVLOG(1) << __func__ << ", state:" << ready_state_;
  DCHECK(thread_checker_.CalledOnValidThread());
  return ready_state_;
}

blink::WebString WebMediaPlayerMS::getErrorMessage() {
  return blink::WebString::fromUTF8(media_log_->GetLastErrorMessage());
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
                             SkPaint& paint) {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  const scoped_refptr<media::VideoFrame> frame =
      compositor_->GetCurrentFrameWithoutUpdatingStatistics();

  media::Context3D context_3d;
  if (frame && frame->HasTextures()) {
    auto* provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
    // GPU Process crashed.
    if (!provider)
      return;
    context_3d = media::Context3D(provider->ContextGL(), provider->GrContext());
    DCHECK(context_3d.gl);
  }
  const gfx::RectF dest_rect(rect.x, rect.y, rect.width, rect.height);
  video_renderer_.Paint(frame, canvas, dest_rect, paint,
                        video_rotation_, context_3d);
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

size_t WebMediaPlayerMS::audioDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerMS::videoDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerMS::OnHidden() {
#if defined(OS_ANDROID)
  DCHECK(thread_checker_.CalledOnValidThread());

  // Method called when the RenderFrame is sent to background and suspended
  // (android). Substitute the displayed VideoFrame with a copy to avoid
  // holding on to it unnecessarily.
  //
  // During undoable tab closures OnHidden() may be called back to back, so we
  // can't rely on |render_frame_suspended_| being false here.
  if (frame_deliverer_) {
    compositor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FrameDeliverer::SetRenderFrameSuspended,
                              base::Unretained(frame_deliverer_.get()), true));
  }

  if (!paused_)
    compositor_->ReplaceCurrentFrameWithACopy();
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnShown() {
#if defined(OS_ANDROID)
  DCHECK(thread_checker_.CalledOnValidThread());

  if (frame_deliverer_) {
    compositor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FrameDeliverer::SetRenderFrameSuspended,
                              base::Unretained(frame_deliverer_.get()), false));
  }

  // Resume playback on visibility. play() clears |should_play_upon_shown_|.
  if (should_play_upon_shown_)
    play();
#endif  // defined(OS_ANDROID)
}

bool WebMediaPlayerMS::OnSuspendRequested(bool must_suspend) {
#if defined(OS_ANDROID)
  if (!must_suspend)
    return false;

  if (!paused_) {
    pause();
    should_play_upon_shown_ = true;
  }

  if (delegate_)
    delegate_->PlayerGone(delegate_id_);

  if (frame_deliverer_) {
    compositor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FrameDeliverer::SetRenderFrameSuspended,
                              base::Unretained(frame_deliverer_.get()), true));
  }
#endif  // defined(OS_ANDROID)
  return true;
}

void WebMediaPlayerMS::OnPlay() {
  // TODO(perkj, magjed): It's not clear how WebRTC should work with an
  // MediaSession, until these issues are resolved, disable session controls.
  // http://crbug.com/595297.
}

void WebMediaPlayerMS::OnPause() {
  // TODO(perkj, magjed): See TODO in OnPlay().
}

void WebMediaPlayerMS::OnVolumeMultiplierUpdate(double multiplier) {
  // TODO(perkj, magjed): See TODO in OnPlay().
}

bool WebMediaPlayerMS::copyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  TRACE_EVENT0("media", "WebMediaPlayerMS:copyVideoTextureToPlatformTexture");
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<media::VideoFrame> video_frame =
      compositor_->GetCurrentFrameWithoutUpdatingStatistics();

  if (!video_frame.get() || !video_frame->HasTextures())
    return false;

  media::Context3D context_3d;
  auto* provider =
      RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
  // GPU Process crashed.
  if (!provider)
    return false;
  context_3d = media::Context3D(provider->ContextGL(), provider->GrContext());
  DCHECK(context_3d.gl);
  return video_renderer_.CopyVideoFrameTexturesToGLTexture(
      context_3d, gl, video_frame.get(), texture, internal_format, type,
      premultiply_alpha, flip_y);
}

void WebMediaPlayerMS::OnFirstFrameReceived(media::VideoRotation video_rotation,
                                            bool is_opaque) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
  SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  OnRotationChanged(video_rotation, is_opaque);
}

void WebMediaPlayerMS::OnOpacityChanged(bool is_opaque) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Opacity can be changed during the session without resetting
  // |video_weblayer_|.
  video_weblayer_->layer()->SetContentsOpaque(is_opaque);
}

void WebMediaPlayerMS::OnRotationChanged(media::VideoRotation video_rotation,
                                         bool is_opaque) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  video_rotation_ = video_rotation;

  video_weblayer_.reset(new cc_blink::WebLayerImpl(
      cc::VideoLayer::Create(compositor_.get(), video_rotation)));
  video_weblayer_->layer()->SetContentsOpaque(is_opaque);
  video_weblayer_->SetContentsOpaqueIsFixed(true);
  get_client()->setWebLayer(video_weblayer_.get());
}

void WebMediaPlayerMS::RepaintInternal() {
  DVLOG(1) << __func__;
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

void WebMediaPlayerMS::TriggerResize() {
  get_client()->sizeChanged();
}

}  // namespace content
