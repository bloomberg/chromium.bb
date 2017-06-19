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
#include "content/child/child_process.h"
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
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerSource.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

// FrameDeliverer is responsible for delivering frames received on
// the IO thread by calling of EnqueueFrame() method of |compositor_|.
//
// It is created on the main thread, but methods should be called and class
// should be destructed on the IO thread.
class WebMediaPlayerMS::FrameDeliverer {
 public:
  FrameDeliverer(const base::WeakPtr<WebMediaPlayerMS>& player,
                 const MediaStreamVideoRenderer::RepaintCB& enqueue_frame_cb)
      : last_frame_opaque_(true),
        last_frame_rotation_(media::VIDEO_ROTATION_0),
        received_first_frame_(false),
        main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        player_(player),
        enqueue_frame_cb_(enqueue_frame_cb),
        weak_factory_(this) {
    io_thread_checker_.DetachFromThread();
  }

  ~FrameDeliverer() { DCHECK(io_thread_checker_.CalledOnValidThread()); }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame) {
    DCHECK(io_thread_checker_.CalledOnValidThread());

#if defined(OS_ANDROID)
    if (render_frame_suspended_)
      return;
#endif  // defined(OS_ANDROID)

    base::TimeTicks render_time;
    if (frame->metadata()->GetTimeTicks(
            media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
      TRACE_EVENT1("webrtc", "WebMediaPlayerMS::OnVideoFrame",
                   "Ideal Render Instant", render_time.ToInternalValue());
    } else {
      TRACE_EVENT0("webrtc", "WebMediaPlayerMS::OnVideoFrame");
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
    DCHECK(io_thread_checker_.CalledOnValidThread());
    render_frame_suspended_ = render_frame_suspended;
  }
#endif  // defined(OS_ANDROID)

  MediaStreamVideoRenderer::RepaintCB GetRepaintCallback() {
    return base::Bind(&FrameDeliverer::OnVideoFrame,
                      weak_factory_.GetWeakPtr());
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
  const MediaStreamVideoRenderer::RepaintCB enqueue_frame_cb_;

  // Used for DCHECKs to ensure method calls are executed on the correct thread.
  base::ThreadChecker io_thread_checker_;

  base::WeakPtrFactory<FrameDeliverer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

WebMediaPlayerMS::WebMediaPlayerMS(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    media::WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::MediaLog> media_log,
    std::unique_ptr<MediaStreamRendererFactory> factory,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::TaskRunner> worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin)
    : frame_(frame),
      network_state_(WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      buffered_(static_cast<size_t>(0)),
      client_(client),
      delegate_(delegate),
      delegate_id_(0),
      paused_(true),
      video_rotation_(media::VIDEO_ROTATION_0),
      media_log_(std::move(media_log)),
      renderer_factory_(std::move(factory)),
      io_task_runner_(io_task_runner),
      compositor_task_runner_(compositor_task_runner),
      media_task_runner_(media_task_runner),
      worker_task_runner_(worker_task_runner),
      gpu_factories_(gpu_factories),
      initial_audio_output_device_id_(sink_id.Utf8()),
      initial_security_origin_(security_origin.IsNull()
                                   ? url::Origin()
                                   : url::Origin(security_origin)),
      volume_(1.0),
      volume_multiplier_(1.0),
      should_play_upon_shown_(false) {
  DVLOG(1) << __func__;
  DCHECK(client);
  DCHECK(delegate_);
  delegate_id_ = delegate_->AddObserver(this);

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerMS::~WebMediaPlayerMS() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Destruct compositor resources in the proper order.
  get_client()->SetWebLayer(nullptr);
  if (video_weblayer_)
    static_cast<cc::VideoLayer*>(video_weblayer_->layer())->StopUsingProvider();

  if (frame_deliverer_)
    io_task_runner_->DeleteSoon(FROM_HERE, frame_deliverer_.release());

  if (compositor_)
    compositor_->StopUsingProvider();

  if (video_frame_provider_)
    video_frame_provider_->Stop();

  if (audio_renderer_)
    audio_renderer_->Stop();

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  delegate_->PlayerGone(delegate_id_);
  delegate_->RemoveObserver(delegate_id_);
}

void WebMediaPlayerMS::Load(LoadType load_type,
                            const blink::WebMediaPlayerSource& source,
                            CORSMode /*cors_mode*/) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(acolwell): Change this to DCHECK_EQ(load_type, LoadTypeMediaStream)
  // once Blink-side changes land.
  DCHECK_NE(load_type, kLoadTypeMediaSource);
  blink::WebMediaStream web_stream =
      GetWebMediaStreamFromWebMediaPlayerSource(source);

  compositor_ = new WebMediaPlayerMSCompositor(
      compositor_task_runner_, io_task_runner_, web_stream, AsWeakPtr());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  std::string stream_id =
      web_stream.IsNull() ? std::string() : web_stream.Id().Utf8();
  media_log_->AddEvent(media_log_->CreateLoadEvent(stream_id));

  frame_deliverer_.reset(new WebMediaPlayerMS::FrameDeliverer(
      AsWeakPtr(),
      base::Bind(&WebMediaPlayerMSCompositor::EnqueueFrame, compositor_)));
  video_frame_provider_ = renderer_factory_->GetVideoRenderer(
      web_stream, media::BindToCurrentLoop(base::Bind(
                      &WebMediaPlayerMS::OnSourceError, AsWeakPtr())),
      frame_deliverer_->GetRepaintCallback(), io_task_runner_,
      media_task_runner_, worker_task_runner_, gpu_factories_);

  RenderFrame* const frame = RenderFrame::FromWebFrame(frame_);

  if (frame) {
    // Report UMA and RAPPOR metrics.
    GURL url = source.IsURL() ? GURL(source.GetAsURL()) : GURL();
    media::ReportMetrics(load_type, url, frame_->GetSecurityOrigin(),
                         media_log_.get());

    audio_renderer_ = renderer_factory_->GetAudioRenderer(
        web_stream, frame->GetRoutingID(), initial_audio_output_device_id_,
        initial_security_origin_);
  }

  if (!video_frame_provider_ && !audio_renderer_) {
    SetNetworkState(WebMediaPlayer::kNetworkStateNetworkError);
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
    SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
    SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
  }
}

void WebMediaPlayerMS::Play() {
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

  if (HasVideo())
    delegate_->DidPlayerSizeChange(delegate_id_, NaturalSize());
  // TODO(perkj, magjed): We use OneShot focus type here so that it takes
  // audio focus once it starts, and then will not respond to further audio
  // focus changes. See http://crbug.com/596516 for more details.
  delegate_->DidPlay(delegate_id_, HasVideo(), HasAudio(),
                     media::MediaContentType::OneShot);
  delegate_->SetIdle(delegate_id_, false);

  paused_ = false;
}

void WebMediaPlayerMS::Pause() {
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

  delegate_->DidPause(delegate_id_);
  delegate_->SetIdle(delegate_id_, true);

  paused_ = true;
}

bool WebMediaPlayerMS::SupportsSave() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

void WebMediaPlayerMS::Seek(double seconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::SetRate(double rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::SetVolume(double volume) {
  DVLOG(1) << __func__ << "(volume=" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  volume_ = volume;
  if (audio_renderer_.get())
    audio_renderer_->SetVolume(volume_ * volume_multiplier_);
  delegate_->DidPlayerMutedStatusChange(delegate_id_, volume == 0.0);
}

void WebMediaPlayerMS::SetSinkId(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callback) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  const media::OutputDeviceStatusCB callback =
      media::ConvertToOutputDeviceStatusCB(web_callback);
  if (audio_renderer_) {
    audio_renderer_->SwitchOutputDevice(sink_id.Utf8(), security_origin,
                                        callback);
  } else {
    callback.Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  }
}

void WebMediaPlayerMS::SetPreload(WebMediaPlayer::Preload preload) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool WebMediaPlayerMS::HasVideo() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (video_frame_provider_.get() != nullptr);
}

bool WebMediaPlayerMS::HasAudio() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (audio_renderer_.get() != nullptr);
}

blink::WebSize WebMediaPlayerMS::NaturalSize() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (video_rotation_ == media::VIDEO_ROTATION_90 ||
      video_rotation_ == media::VideoRotation::VIDEO_ROTATION_270) {
    const gfx::Size& current_size = compositor_->GetCurrentSize();
    return blink::WebSize(current_size.height(), current_size.width());
  }
  return blink::WebSize(compositor_->GetCurrentSize());
}

bool WebMediaPlayerMS::Paused() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return paused_;
}

bool WebMediaPlayerMS::Seeking() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

double WebMediaPlayerMS::Duration() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::numeric_limits<double>::infinity();
}

double WebMediaPlayerMS::CurrentTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::TimeDelta current_time = compositor_->GetCurrentTime();
  if (current_time.ToInternalValue() != 0)
    return current_time.InSecondsF();
  else if (audio_renderer_.get())
    return audio_renderer_->GetCurrentRenderTime().InSecondsF();
  return 0.0;
}

blink::WebMediaPlayer::NetworkState WebMediaPlayerMS::GetNetworkState() const {
  DVLOG(1) << __func__ << ", state:" << network_state_;
  DCHECK(thread_checker_.CalledOnValidThread());
  return network_state_;
}

blink::WebMediaPlayer::ReadyState WebMediaPlayerMS::GetReadyState() const {
  DVLOG(1) << __func__ << ", state:" << ready_state_;
  DCHECK(thread_checker_.CalledOnValidThread());
  return ready_state_;
}

blink::WebString WebMediaPlayerMS::GetErrorMessage() const {
  return blink::WebString::FromUTF8(media_log_->GetErrorMessage());
}

blink::WebTimeRanges WebMediaPlayerMS::Buffered() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return buffered_;
}

blink::WebTimeRanges WebMediaPlayerMS::Seekable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return blink::WebTimeRanges();
}

bool WebMediaPlayerMS::DidLoadingProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void WebMediaPlayerMS::Paint(blink::WebCanvas* canvas,
                             const blink::WebRect& rect,
                             cc::PaintFlags& flags) {
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
  video_renderer_.Paint(frame, canvas, dest_rect, flags, video_rotation_,
                        context_3d);
}

bool WebMediaPlayerMS::HasSingleSecurityOrigin() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

bool WebMediaPlayerMS::DidPassCORSAccessCheck() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

double WebMediaPlayerMS::MediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerMS::DecodedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return compositor_->total_frame_count();
}

unsigned WebMediaPlayerMS::DroppedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return compositor_->dropped_frame_count();
}

size_t WebMediaPlayerMS::AudioDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerMS::VideoDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerMS::OnFrameHidden() {
#if defined(OS_ANDROID)
  DCHECK(thread_checker_.CalledOnValidThread());

  // Method called when the RenderFrame is sent to background and suspended
  // (android). Substitute the displayed VideoFrame with a copy to avoid
  // holding on to it unnecessarily.
  //
  // During undoable tab closures OnHidden() may be called back to back, so we
  // can't rely on |render_frame_suspended_| being false here.
  if (frame_deliverer_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FrameDeliverer::SetRenderFrameSuspended,
                              base::Unretained(frame_deliverer_.get()), true));
  }

  if (!paused_)
    compositor_->ReplaceCurrentFrameWithACopy();
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnFrameClosed() {
#if defined(OS_ANDROID)
  if (!paused_) {
    Pause();
    should_play_upon_shown_ = true;
  }

  delegate_->PlayerGone(delegate_id_);

  if (frame_deliverer_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FrameDeliverer::SetRenderFrameSuspended,
                              base::Unretained(frame_deliverer_.get()), true));
  }
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnFrameShown() {
#if defined(OS_ANDROID)
  DCHECK(thread_checker_.CalledOnValidThread());

  if (frame_deliverer_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FrameDeliverer::SetRenderFrameSuspended,
                              base::Unretained(frame_deliverer_.get()), false));
  }

  // Resume playback on visibility. play() clears |should_play_upon_shown_|.
  if (should_play_upon_shown_)
    Play();
#endif  // defined(OS_ANDROID)
}

void WebMediaPlayerMS::OnIdleTimeout() {}

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

void WebMediaPlayerMS::OnBecamePersistentVideo(bool value) {
  get_client()->OnBecamePersistentVideo(value);
}

bool WebMediaPlayerMS::CopyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned target,
    unsigned int texture,
    unsigned internal_format,
    unsigned format,
    unsigned type,
    int level,
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
      context_3d, gl, video_frame.get(), target, texture, internal_format,
      format, type, level, premultiply_alpha, flip_y);
}

bool WebMediaPlayerMS::TexImageImpl(TexImageFunctionID functionID,
                                    unsigned target,
                                    gpu::gles2::GLES2Interface* gl,
                                    unsigned int texture,
                                    int level,
                                    int internalformat,
                                    unsigned format,
                                    unsigned type,
                                    int xoffset,
                                    int yoffset,
                                    int zoffset,
                                    bool flip_y,
                                    bool premultiply_alpha) {
  TRACE_EVENT0("media", "WebMediaPlayerMS:texImageImpl");
  DCHECK(thread_checker_.CalledOnValidThread());

  const scoped_refptr<media::VideoFrame> video_frame =
      compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  if (!video_frame || !video_frame->IsMappable() ||
      video_frame->HasTextures() ||
      video_frame->format() != media::PIXEL_FORMAT_Y16) {
    return false;
  }

  if (functionID == kTexImage2D) {
    auto* provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
    // GPU Process crashed.
    if (!provider)
      return false;
    return media::SkCanvasVideoRenderer::TexImage2D(
        target, texture, gl, provider->ContextCapabilities(), video_frame.get(),
        level, internalformat, format, type, flip_y, premultiply_alpha);
  } else if (functionID == kTexSubImage2D) {
    return media::SkCanvasVideoRenderer::TexSubImage2D(
        target, gl, video_frame.get(), level, format, type, xoffset, yoffset,
        flip_y, premultiply_alpha);
  }
  return false;
}

void WebMediaPlayerMS::OnFirstFrameReceived(media::VideoRotation video_rotation,
                                            bool is_opaque) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);

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

  std::unique_ptr<cc_blink::WebLayerImpl> rotated_weblayer =
      base::WrapUnique(new cc_blink::WebLayerImpl(
          cc::VideoLayer::Create(compositor_.get(), video_rotation)));
  rotated_weblayer->layer()->SetContentsOpaque(is_opaque);
  rotated_weblayer->SetContentsOpaqueIsFixed(true);
  get_client()->SetWebLayer(rotated_weblayer.get());
  video_weblayer_ = std::move(rotated_weblayer);
}

void WebMediaPlayerMS::RepaintInternal() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  get_client()->Repaint();
}

void WebMediaPlayerMS::OnSourceError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
  RepaintInternal();
}

void WebMediaPlayerMS::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  get_client()->NetworkStateChanged();
}

void WebMediaPlayerMS::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  get_client()->ReadyStateChanged();
}

media::SkCanvasVideoRenderer* WebMediaPlayerMS::GetSkCanvasVideoRenderer() {
  return &video_renderer_;
}

void WebMediaPlayerMS::ResetCanvasCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_renderer_.ResetCache();
}

void WebMediaPlayerMS::TriggerResize() {
  get_client()->SizeChanged();

  delegate_->DidPlayerSizeChange(delegate_id_, NaturalSize());
}

}  // namespace content
