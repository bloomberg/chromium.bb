// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediaplayer_android.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/video_layer.h"
#include "content/renderer/media/android/proxy_media_keys.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/webmediaplayer_proxy_android.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/android/media_player_android.h"
#include "media/base/bind_to_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"

#if defined(GOOGLE_TV)
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/media_stream_client.h"
#endif

static const uint32 kGLTextureExternalOES = 0x8D65;

using WebKit::WebMediaPlayer;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTimeRanges;
using WebKit::WebURL;
using media::MediaPlayerAndroid;
using media::VideoFrame;

namespace {
// Prefix for histograms related to Encrypted Media Extensions.
const char* kMediaEme = "Media.EME.";
}  // namespace

namespace content {

#define BIND_TO_RENDER_LOOP(function) \
  media::BindToLoop(main_loop_, base::Bind(function, AsWeakPtr()))

WebMediaPlayerAndroid::WebMediaPlayerAndroid(
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    RendererMediaPlayerManager* manager,
    WebMediaPlayerProxyAndroid* proxy,
    StreamTextureFactory* factory,
    const scoped_refptr<base::MessageLoopProxy>& media_loop,
    media::MediaLog* media_log)
    : frame_(frame),
      client_(client),
      delegate_(delegate),
      buffered_(1u),
      main_loop_(base::MessageLoopProxy::current()),
      media_loop_(media_loop),
      ignore_metadata_duration_change_(false),
      pending_seek_(0),
      seeking_(false),
      did_loading_progress_(false),
      manager_(manager),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      texture_id_(0),
      texture_mailbox_sync_point_(0),
      stream_id_(0),
      is_playing_(false),
      needs_establish_peer_(true),
      stream_texture_proxy_initialized_(false),
      has_size_info_(false),
      has_media_metadata_(false),
      has_media_info_(false),
      stream_texture_factory_(factory),
      needs_external_surface_(false),
      video_frame_provider_client_(NULL),
#if defined(GOOGLE_TV)
      external_surface_threshold_(-1),
      demuxer_(NULL),
      media_stream_client_(NULL),
#endif  // defined(GOOGLE_TV)
      source_type_(MediaPlayerAndroid::SOURCE_TYPE_URL),
      proxy_(proxy),
      current_time_(0),
      media_log_(media_log) {
  DCHECK(proxy_);
  DCHECK(manager_);

  // We want to be notified of |main_loop_| destruction.
  base::MessageLoop::current()->AddDestructionObserver(this);

  player_id_ = manager_->RegisterMediaPlayer(this);

#if defined(GOOGLE_TV)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseExternalVideoSurfaceThresholdInPixels)) {
    if (!base::StringToInt(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kUseExternalVideoSurfaceThresholdInPixels),
        &external_surface_threshold_)) {
      external_surface_threshold_ = -1;
    }
  }

  // Defer stream texture creation until we are sure it's necessary.
  stream_id_ = 0;
  needs_establish_peer_ = false;
  current_frame_ = VideoFrame::CreateBlackFrame(gfx::Size(1, 1));
#endif
  if (stream_texture_factory_) {
    stream_texture_proxy_.reset(stream_texture_factory_->CreateProxy());
    if (needs_establish_peer_) {
      stream_id_ = stream_texture_factory_->CreateStreamTexture(
          kGLTextureExternalOES,
          &texture_id_,
          &texture_mailbox_,
          &texture_mailbox_sync_point_);
      ReallocateVideoFrame();
    }
  }

  if (WebKit::WebRuntimeFeatures::isLegacyEncryptedMediaEnabled()) {
    // TODO(xhwang): Report an error when there is encrypted stream but EME is
    // not enabled. Currently the player just doesn't start and waits for ever.
    decryptor_.reset(new ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
        client,
        frame,
#else
        proxy_,
        player_id_,  // TODO(xhwang): Use media_keys_id when MediaKeys are
                     // separated from WebMediaPlayer.
#endif // defined(ENABLE_PEPPER_CDMS)
        // |decryptor_| is owned, so Unretained() is safe here.
        base::Bind(&WebMediaPlayerAndroid::OnKeyAdded, base::Unretained(this)),
        base::Bind(&WebMediaPlayerAndroid::OnKeyError, base::Unretained(this)),
        base::Bind(&WebMediaPlayerAndroid::OnKeyMessage,
                   base::Unretained(this))));
  }
}

WebMediaPlayerAndroid::~WebMediaPlayerAndroid() {
  SetVideoFrameProviderClient(NULL);
  client_->setWebLayer(NULL);

  if (proxy_)
    proxy_->DestroyPlayer(player_id_);

  if (stream_id_)
    stream_texture_factory_->DestroyStreamTexture(texture_id_);

  if (manager_)
    manager_->UnregisterMediaPlayer(player_id_);

  if (base::MessageLoop::current())
    base::MessageLoop::current()->RemoveDestructionObserver(this);

  if (source_type_ == MediaPlayerAndroid::SOURCE_TYPE_MSE && delegate_)
    delegate_->PlayerGone(this);

#if defined(GOOGLE_TV)
  if (audio_renderer_) {
    if (audio_renderer_->IsLocalRenderer()) {
      audio_renderer_->Stop();
    } else if (!paused()) {
      // The |audio_renderer_| can be shared by multiple remote streams, and
      // it will be stopped when WebRtcAudioDeviceImpl goes away. So we simply
      // pause the |audio_renderer_| here to avoid re-creating the
      // |audio_renderer_|.
      audio_renderer_->Pause();
    }
  }
  if (demuxer_ && !destroy_demuxer_cb_.is_null()) {
    media_source_delegate_.reset();
    destroy_demuxer_cb_.Run();
  }
#endif
}

void WebMediaPlayerAndroid::load(LoadType load_type,
                                 const WebKit::WebURL& url,
                                 CORSMode cors_mode) {
#if defined(GOOGLE_TV)
  // TODO(acolwell): Remove this hack once Blink-side changes land.
  if (load_type == LoadTypeURL && media_stream_client_)
    load_type = LoadTypeMediaStream;
#endif

  switch (load_type) {
    case LoadTypeURL:
      source_type_ = MediaPlayerAndroid::SOURCE_TYPE_URL;
      break;
    case LoadTypeMediaSource:
      source_type_ = MediaPlayerAndroid::SOURCE_TYPE_MSE;
      break;
    case LoadTypeMediaStream:
#if defined(GOOGLE_TV)
      source_type_ = MediaPlayerAndroid::SOURCE_TYPE_MSE;
#else
      source_type_ = MediaPlayerAndroid::SOURCE_TYPE_URL;
#endif
      break;
  }

  has_media_metadata_ = false;
  has_media_info_ = false;

  media::SetDecryptorReadyCB set_decryptor_ready_cb;
  if (decryptor_) {  // |decryptor_| can be NULL is EME if not enabled.
    set_decryptor_ready_cb = base::Bind(&ProxyDecryptor::SetDecryptorReadyCB,
                                        base::Unretained(decryptor_.get()));
  }

  if (source_type_ != MediaPlayerAndroid::SOURCE_TYPE_URL) {
    has_media_info_ = true;
    media_source_delegate_.reset(
        new MediaSourceDelegate(proxy_, player_id_, media_loop_, media_log_));
    // |media_source_delegate_| is owned, so Unretained() is safe here.
    if (source_type_ == MediaPlayerAndroid::SOURCE_TYPE_MSE) {
      media_source_delegate_->InitializeMediaSource(
          base::Bind(&WebMediaPlayerAndroid::OnMediaSourceOpened,
                     base::Unretained(this)),
          base::Bind(&WebMediaPlayerAndroid::OnNeedKey, base::Unretained(this)),
          set_decryptor_ready_cb,
          base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                     base::Unretained(this)),
          BIND_TO_RENDER_LOOP(&WebMediaPlayerAndroid::OnDurationChange));
    }
#if defined(GOOGLE_TV)
    // TODO(xhwang): Pass set_decryptor_ready_cb in InitializeMediaStream() to
    // enable ClearKey support for Google TV.
    if (source_type_ == MediaPlayerAndroid::SOURCE_TYPE_STREAM) {
      media_source_delegate_->InitializeMediaStream(
          demuxer_,
          base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                     base::Unretained(this)));
      audio_renderer_ = media_stream_client_->GetAudioRenderer(url);
      if (audio_renderer_)
        audio_renderer_->Start();
    }
#endif
  } else {
    info_loader_.reset(
        new MediaInfoLoader(
            url,
            cors_mode,
            base::Bind(&WebMediaPlayerAndroid::DidLoadMediaInfo,
                       base::Unretained(this))));
    info_loader_->Start(frame_);
  }

  InitializeMediaPlayer(url);
}

void WebMediaPlayerAndroid::InitializeMediaPlayer(const WebURL& url) {
  url_ = url;
  GURL first_party_url = frame_->document().firstPartyForCookies();
  proxy_->Initialize(player_id_, url, source_type_, first_party_url);
  if (manager_->IsInFullscreen(frame_))
    proxy_->EnterFullscreen(player_id_);

  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveNothing);
}

void WebMediaPlayerAndroid::DidLoadMediaInfo(
    MediaInfoLoader::Status status) {
  DCHECK(!media_source_delegate_);
  if (status == MediaInfoLoader::kFailed) {
    info_loader_.reset();
    UpdateNetworkState(WebMediaPlayer::NetworkStateNetworkError);
    return;
  }

  has_media_info_ = true;
  if (has_media_metadata_ &&
      ready_state_ != WebMediaPlayer::ReadyStateHaveEnoughData) {
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
  }
}

void WebMediaPlayerAndroid::play() {
#if defined(GOOGLE_TV)
  if (hasVideo() && needs_external_surface_ &&
      !manager_->IsInFullscreen(frame_)) {
    DCHECK(!needs_establish_peer_);
    proxy_->RequestExternalSurface(player_id_, last_computed_rect_);
  }
  if (audio_renderer_ && paused())
    audio_renderer_->Play();
#endif
  if (hasVideo() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

  if (paused())
    proxy_->Start(player_id_);
  UpdatePlayingState(true);
}

void WebMediaPlayerAndroid::pause() {
#if defined(GOOGLE_TV)
  if (audio_renderer_ && !paused())
    audio_renderer_->Pause();
#endif
  proxy_->Pause(player_id_);
  UpdatePlayingState(false);
}

void WebMediaPlayerAndroid::seek(double seconds) {
  pending_seek_ = seconds;
  seeking_ = true;

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);
  proxy_->Seek(player_id_, seek_time);
}

bool WebMediaPlayerAndroid::supportsFullscreen() const {
  return true;
}

bool WebMediaPlayerAndroid::supportsSave() const {
  return false;
}

void WebMediaPlayerAndroid::setRate(double rate) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::setVolume(double volume) {
  proxy_->SetVolume(player_id_, volume);
}

bool WebMediaPlayerAndroid::hasVideo() const {
  // If we have obtained video size information before, use it.
  if (has_size_info_)
    return !natural_size_.isEmpty();

  // TODO(qinmin): need a better method to determine whether the current media
  // content contains video. Android does not provide any function to do
  // this.
  // We don't know whether the current media content has video unless
  // the player is prepared. If the player is not prepared, we fall back
  // to the mime-type. There may be no mime-type on a redirect URL.
  // In that case, we conservatively assume it contains video so that
  // enterfullscreen call will not fail.
  if (!url_.has_path())
    return false;
  std::string mime;
  if(!net::GetMimeTypeFromFile(base::FilePath(url_.path()), &mime))
    return true;
  return mime.find("audio/") == std::string::npos;
}

bool WebMediaPlayerAndroid::hasAudio() const {
  // TODO(hclam): Query status of audio and return the actual value.
  return true;
}

bool WebMediaPlayerAndroid::paused() const {
  return !is_playing_;
}

bool WebMediaPlayerAndroid::seeking() const {
  return seeking_;
}

double WebMediaPlayerAndroid::duration() const {
  // HTML5 spec requires duration to be NaN if readyState is HAVE_NOTHING
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  // TODO(wolenetz): Correctly handle durations that MediaSourcePlayer
  // considers unseekable, including kInfiniteDuration().
  // See http://crbug.com/248396
  return duration_.InSecondsF();
}

double WebMediaPlayerAndroid::currentTime() const {
  // If the player is pending for a seek, return the seek time.
  if (seeking())
    return pending_seek_;
  return current_time_;
}

WebSize WebMediaPlayerAndroid::naturalSize() const {
  return natural_size_;
}

WebMediaPlayer::NetworkState WebMediaPlayerAndroid::networkState() const {
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerAndroid::readyState() const {
  return ready_state_;
}

const WebTimeRanges& WebMediaPlayerAndroid::buffered() {
  if (media_source_delegate_)
    return media_source_delegate_->Buffered();
  return buffered_;
}

double WebMediaPlayerAndroid::maxTimeSeekable() const {
  // If we haven't even gotten to ReadyStateHaveMetadata yet then just
  // return 0 so that the seekable range is empty.
  if (ready_state_ < WebMediaPlayer::ReadyStateHaveMetadata)
    return 0.0;

  // TODO(hclam): If this stream is not seekable this should return 0.
  return duration();
}

bool WebMediaPlayerAndroid::didLoadingProgress() const {
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

void WebMediaPlayerAndroid::paint(WebKit::WebCanvas* canvas,
                                  const WebKit::WebRect& rect,
                                  unsigned char alpha) {
  NOTIMPLEMENTED();
}

bool WebMediaPlayerAndroid::copyVideoTextureToPlatformTexture(
    WebKit::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  if (!texture_id_)
    return false;

  // For hidden video element (with style "display:none"), ensure the texture
  // size is set.
  if (cached_stream_texture_size_.width != natural_size_.width ||
      cached_stream_texture_size_.height != natural_size_.height) {
    stream_texture_factory_->SetStreamTextureSize(
        stream_id_, gfx::Size(natural_size_.width, natural_size_.height));
    cached_stream_texture_size_ = natural_size_;
  }

  // Ensure the target of texture is set before copyTextureCHROMIUM, otherwise
  // an invalid texture target may be used for copy texture.
  web_graphics_context->bindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id_);

  // The video is stored in an unmultiplied format, so premultiply if
  // necessary.
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    premultiply_alpha);

  // Application itself needs to take care of setting the right flip_y
  // value down to get the expected result.
  // flip_y==true means to reverse the video orientation while
  // flip_y==false means to keep the intrinsic orientation.
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
  web_graphics_context->copyTextureCHROMIUM(GL_TEXTURE_2D, texture_id_,
                                            texture, level, internal_format,
                                            type);
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, false);
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    false);

  web_graphics_context->bindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  return true;
}

bool WebMediaPlayerAndroid::hasSingleSecurityOrigin() const {
  if (info_loader_)
    return info_loader_->HasSingleOrigin();
  // The info loader may have failed.
  if (source_type_ == MediaPlayerAndroid::SOURCE_TYPE_URL)
    return false;
  return true;
}

bool WebMediaPlayerAndroid::didPassCORSAccessCheck() const {
  if (info_loader_)
    return info_loader_->DidPassCORSAccessCheck();
  return false;
}

double WebMediaPlayerAndroid::mediaTimeForTimeValue(double timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerAndroid::decodedFrameCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->DecodedFrameCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::droppedFrameCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->DroppedFrameCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::audioDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->AudioDecodedByteCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::videoDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->VideoDecodedByteCount();
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerAndroid::OnMediaMetadataChanged(
    base::TimeDelta duration, int width, int height, bool success) {
  bool need_to_signal_duration_changed = false;

  if (url_.SchemeIs("file"))
    UpdateNetworkState(WebMediaPlayer::NetworkStateLoaded);

  // Update duration, if necessary, prior to ready state updates that may
  // cause duration() query.
  // TODO(wolenetz): Correctly handle durations that MediaSourcePlayer
  // considers unseekable, including kInfiniteDuration().
  // See http://crbug.com/248396
  if (!ignore_metadata_duration_change_ && duration_ != duration) {
    duration_ = duration;

    // Client readyState transition from HAVE_NOTHING to HAVE_METADATA
    // already triggers a durationchanged event. If this is a different
    // transition, remember to signal durationchanged.
    // Do not ever signal durationchanged on metadata change in MSE case
    // because OnDurationChange() handles this.
    if (ready_state_ > WebMediaPlayer::ReadyStateHaveNothing &&
        source_type_ != MediaPlayerAndroid::SOURCE_TYPE_MSE) {
      need_to_signal_duration_changed = true;
    }
  }

  has_media_metadata_ = true;
  if (has_media_info_ &&
      ready_state_ != WebMediaPlayer::ReadyStateHaveEnoughData) {
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
  }

  // TODO(wolenetz): Should we just abort early and set network state to an
  // error if success == false? See http://crbug.com/248399
  if (success)
    OnVideoSizeChanged(width, height);

  if (hasVideo() && !video_weblayer_ && client_->needsWebLayerForVideo()) {
    video_weblayer_.reset(
        new webkit::WebLayerImpl(cc::VideoLayer::Create(this)));
    client_->setWebLayer(video_weblayer_.get());
  }

  if (need_to_signal_duration_changed)
    client_->durationChanged();
}

void WebMediaPlayerAndroid::OnPlaybackComplete() {
  // When playback is about to finish, android media player often stops
  // at a time which is smaller than the duration. This makes webkit never
  // know that the playback has finished. To solve this, we set the
  // current time to media duration when OnPlaybackComplete() get called.
  OnTimeUpdate(duration_);
  client_->timeChanged();
}

void WebMediaPlayerAndroid::OnBufferingUpdate(int percentage) {
  buffered_[0].end = duration() * percentage / 100;
  did_loading_progress_ = true;
}

void WebMediaPlayerAndroid::OnSeekComplete(base::TimeDelta current_time) {
  seeking_ = false;

  OnTimeUpdate(current_time);

  UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  client_->timeChanged();
}

void WebMediaPlayerAndroid::OnMediaError(int error_type) {
  switch (error_type) {
    case MediaPlayerAndroid::MEDIA_ERROR_FORMAT:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_DECODE:
      UpdateNetworkState(WebMediaPlayer::NetworkStateDecodeError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_INVALID_CODE:
      break;
  }
  client_->repaint();
}

void WebMediaPlayerAndroid::OnVideoSizeChanged(int width, int height) {
  has_size_info_ = true;
  if (natural_size_.width == width && natural_size_.height == height)
    return;

#if defined(GOOGLE_TV)
  if ((external_surface_threshold_ >= 0 &&
       external_surface_threshold_ <= width * height) ||
      // Use H/W surface for MSE as the content is protected.
      media_source_delegate_) {
    needs_external_surface_ = true;
    if (!paused() && !manager_->IsInFullscreen(frame_))
      proxy_->RequestExternalSurface(player_id_, last_computed_rect_);
  } else if (stream_texture_factory_ && !stream_texture_proxy_) {
    // Do deferred stream texture creation finally.
    if (paused()) {
      stream_id_ = stream_texture_factory_->CreateStreamTexture(
          kGLTextureExternalOES,
          &texture_id_,
          &texture_mailbox_,
          &texture_mailbox_sync_point_);
      SetNeedsEstablishPeer(true);
    } else {
      EstablishSurfaceTexturePeer();
    }
  }
#endif

  natural_size_.width = width;
  natural_size_.height = height;
  ReallocateVideoFrame();
}

void WebMediaPlayerAndroid::OnTimeUpdate(base::TimeDelta current_time) {
  current_time_ = current_time.InSecondsF();
}

void WebMediaPlayerAndroid::OnDidEnterFullscreen() {
  if (!manager_->IsInFullscreen(frame_)) {
    frame_->view()->willEnterFullScreen();
    frame_->view()->didEnterFullScreen();
    manager_->DidEnterFullscreen(frame_);
  }
}

void WebMediaPlayerAndroid::OnDidExitFullscreen() {
  // |needs_external_surface_| is always false on non-TV devices.
  if (!needs_external_surface_)
    SetNeedsEstablishPeer(true);
  // We had the fullscreen surface connected to Android MediaPlayer,
  // so reconnect our surface texture for embedded playback.
  if (!paused() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

#if defined(GOOGLE_TV)
  if (!paused() && needs_external_surface_)
    proxy_->RequestExternalSurface(player_id_, last_computed_rect_);
#endif

  frame_->view()->willExitFullScreen();
  frame_->view()->didExitFullScreen();
  manager_->DidExitFullscreen();
  client_->repaint();
}

void WebMediaPlayerAndroid::OnMediaPlayerPlay() {
  UpdatePlayingState(true);
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnMediaPlayerPause() {
  UpdatePlayingState(false);
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnMediaSeekRequest(base::TimeDelta time_to_seek,
                                               unsigned seek_request_id) {
  if (!media_source_delegate_)
    return;

  media_source_delegate_->Seek(time_to_seek, seek_request_id);
  OnTimeUpdate(time_to_seek);
}

void WebMediaPlayerAndroid::OnMediaConfigRequest() {
  if (!media_source_delegate_)
    return;

  media_source_delegate_->OnMediaConfigRequest();
}

void WebMediaPlayerAndroid::OnDurationChange(const base::TimeDelta& duration) {
  // Only MSE |source_type_| registers this callback.
  DCHECK(source_type_ == MediaPlayerAndroid::SOURCE_TYPE_MSE);

  // Cache the new duration value and trust it over any subsequent duration
  // values received in OnMediaMetadataChanged().
  // TODO(wolenetz): Correctly handle durations that MediaSourcePlayer
  // considers unseekable, including kInfiniteDuration().
  // See http://crbug.com/248396
  duration_ = duration;
  ignore_metadata_duration_change_ = true;

  // Send message to Android MediaSourcePlayer to update duration.
  if (proxy_)
    proxy_->DurationChanged(player_id_, duration_);

  // Notify MediaPlayerClient that duration has changed, if > HAVE_NOTHING.
  if (ready_state_ > WebMediaPlayer::ReadyStateHaveNothing)
    client_->durationChanged();
}

void WebMediaPlayerAndroid::UpdateNetworkState(
    WebMediaPlayer::NetworkState state) {
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing &&
      (state == WebMediaPlayer::NetworkStateNetworkError ||
       state == WebMediaPlayer::NetworkStateDecodeError)) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    network_state_ = WebMediaPlayer::NetworkStateFormatError;
  } else {
    network_state_ = state;
  }
  client_->networkStateChanged();
}

void WebMediaPlayerAndroid::UpdateReadyState(
    WebMediaPlayer::ReadyState state) {
  ready_state_ = state;
  client_->readyStateChanged();
}

void WebMediaPlayerAndroid::OnPlayerReleased() {
  // |needs_external_surface_| is always false on non-TV devices.
  if (!needs_external_surface_)
    needs_establish_peer_ = true;

#if defined(GOOGLE_TV)
  last_computed_rect_ = gfx::RectF();
#endif
}

void WebMediaPlayerAndroid::ReleaseMediaResources() {
  switch (network_state_) {
    // Pause the media player and inform WebKit if the player is in a good
    // shape.
    case WebMediaPlayer::NetworkStateIdle:
    case WebMediaPlayer::NetworkStateLoading:
    case WebMediaPlayer::NetworkStateLoaded:
      pause();
      client_->playbackStateChanged();
      break;
    // If a WebMediaPlayer instance has entered into one of these states,
    // the internal network state in HTMLMediaElement could be set to empty.
    // And calling playbackStateChanged() could get this object deleted.
    case WebMediaPlayer::NetworkStateEmpty:
    case WebMediaPlayer::NetworkStateFormatError:
    case WebMediaPlayer::NetworkStateNetworkError:
    case WebMediaPlayer::NetworkStateDecodeError:
      break;
  }
  proxy_->ReleaseResources(player_id_);
  OnPlayerReleased();
}

void WebMediaPlayerAndroid::WillDestroyCurrentMessageLoop() {
  if (manager_)
    manager_->UnregisterMediaPlayer(player_id_);
  Detach();
}

void WebMediaPlayerAndroid::Detach() {
  if (stream_id_) {
    stream_texture_factory_->DestroyStreamTexture(texture_id_);
    stream_id_ = 0;
  }

  media_source_delegate_.reset();
  current_frame_ = NULL;
  manager_ = NULL;
  proxy_ = NULL;
}

void WebMediaPlayerAndroid::ReallocateVideoFrame() {
  if (needs_external_surface_) {
    // VideoFrame::CreateHoleFrame is only defined under GOOGLE_TV.
#if defined(GOOGLE_TV)
    if (!natural_size_.isEmpty()) {
      current_frame_ = VideoFrame::CreateHoleFrame(natural_size_);
      // Force the client to grab the hole frame.
      client_->repaint();
    }
#else
    NOTIMPLEMENTED() << "Hole punching not supported outside of Google TV";
#endif
  } else if (texture_id_) {
    current_frame_ = VideoFrame::WrapNativeTexture(
        new VideoFrame::MailboxHolder(
            texture_mailbox_,
            texture_mailbox_sync_point_,
            VideoFrame::MailboxHolder::TextureNoLongerNeededCallback()),
        kGLTextureExternalOES, natural_size_,
        gfx::Rect(natural_size_), natural_size_, base::TimeDelta(),
        VideoFrame::ReadPixelsCB(),
        base::Closure());
  }
}

void WebMediaPlayerAndroid::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  // This is called from both the main renderer thread and the compositor
  // thread (when the main thread is blocked).
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = client;

  // Set the callback target when a frame is produced.
  if (stream_texture_proxy_)
    stream_texture_proxy_->SetClient(client);
}

scoped_refptr<media::VideoFrame> WebMediaPlayerAndroid::GetCurrentFrame() {
  if (!stream_texture_proxy_initialized_ && stream_texture_proxy_ &&
      stream_id_ && !needs_external_surface_) {
    gfx::Size natural_size = current_frame_->natural_size();
    stream_texture_proxy_->BindToCurrentThread(stream_id_);
    stream_texture_factory_->SetStreamTextureSize(stream_id_, natural_size);
    stream_texture_proxy_initialized_ = true;
    cached_stream_texture_size_ = natural_size;
  }
  return current_frame_;
}

void WebMediaPlayerAndroid::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

void WebMediaPlayerAndroid::EstablishSurfaceTexturePeer() {
  if (media_source_delegate_ && stream_texture_factory_) {
    // MediaCodec will release the old surface when it goes away, we need to
    // recreate a new one each time this is called.
    stream_texture_factory_->DestroyStreamTexture(texture_id_);
    stream_id_ = 0;
    texture_id_ = 0;
    texture_mailbox_ = gpu::Mailbox();
    texture_mailbox_sync_point_ = 0;
    stream_id_ = stream_texture_factory_->CreateStreamTexture(
        kGLTextureExternalOES,
        &texture_id_,
        &texture_mailbox_,
        &texture_mailbox_sync_point_);
    ReallocateVideoFrame();
    stream_texture_proxy_initialized_ = false;
  }
  if (stream_texture_factory_.get() && stream_id_)
    stream_texture_factory_->EstablishPeer(stream_id_, player_id_);
  needs_establish_peer_ = false;
}

void WebMediaPlayerAndroid::SetNeedsEstablishPeer(bool needs_establish_peer) {
  needs_establish_peer_ = needs_establish_peer;
}

void WebMediaPlayerAndroid::UpdatePlayingState(bool is_playing) {
  is_playing_ = is_playing;
  if (!delegate_)
    return;
  if (is_playing)
    delegate_->DidPlay(this);
  else
    delegate_->DidPause(this);
}

#if defined(GOOGLE_TV)
bool WebMediaPlayerAndroid::RetrieveGeometryChange(gfx::RectF* rect) {
  if (!video_weblayer_)
    return false;

  // Compute the geometry of video frame layer.
  cc::Layer* layer = video_weblayer_->layer();
  rect->set_size(layer->bounds());
  while (layer) {
    rect->Offset(layer->position().OffsetFromOrigin());
    layer = layer->parent();
  }

  // Return false when the geometry hasn't been changed from the last time.
  if (last_computed_rect_ == *rect)
    return false;

  // Store the changed geometry information when it is actually changed.
  last_computed_rect_ = *rect;
  return true;
}
#endif

// The following EME related code is copied from WebMediaPlayerImpl.
// TODO(xhwang): Remove duplicate code between WebMediaPlayerAndroid and
// WebMediaPlayerImpl.
// TODO(kjyoun): Update Google TV EME implementation to use IPC.

// Helper functions to report media EME related stats to UMA. They follow the
// convention of more commonly used macros UMA_HISTOGRAM_ENUMERATION and
// UMA_HISTOGRAM_COUNTS. The reason that we cannot use those macros directly is
// that UMA_* macros require the names to be constant throughout the process'
// lifetime.
static void EmeUMAHistogramEnumeration(const WebKit::WebString& key_system,
                                       const std::string& method,
                                       int sample,
                                       int boundary_value) {
  base::LinearHistogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, boundary_value, boundary_value + 1,
      base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

static void EmeUMAHistogramCounts(const WebKit::WebString& key_system,
                                  const std::string& method,
                                  int sample) {
  // Use the same parameters as UMA_HISTOGRAM_COUNTS.
  base::Histogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, 1000000, 50, base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

// Helper enum for reporting generateKeyRequest/addKey histograms.
enum MediaKeyException {
  kUnknownResultId,
  kSuccess,
  kKeySystemNotSupported,
  kInvalidPlayerState,
  kMaxMediaKeyException
};

static MediaKeyException MediaKeyExceptionForUMA(
    WebMediaPlayer::MediaKeyException e) {
  switch (e) {
    case WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported:
      return kKeySystemNotSupported;
    case WebMediaPlayer::MediaKeyExceptionInvalidPlayerState:
      return kInvalidPlayerState;
    case WebMediaPlayer::MediaKeyExceptionNoError:
      return kSuccess;
    default:
      return kUnknownResultId;
  }
}

// Helper for converting |key_system| name and exception |e| to a pair of enum
// values from above, for reporting to UMA.
static void ReportMediaKeyExceptionToUMA(
    const std::string& method,
    const WebString& key_system,
    WebMediaPlayer::MediaKeyException e) {
  MediaKeyException result_id = MediaKeyExceptionForUMA(e);
  DCHECK_NE(result_id, kUnknownResultId) << e;
  EmeUMAHistogramEnumeration(
      key_system, method, result_id, kMaxMediaKeyException);
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::generateKeyRequest(
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  WebMediaPlayer::MediaKeyException e =
      GenerateKeyRequestInternal(key_system, init_data, init_data_length);
  ReportMediaKeyExceptionToUMA("generateKeyRequest", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerAndroid::GenerateKeyRequestInternal(
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  DVLOG(1) << "generateKeyRequest: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.isEmpty()) {
    if (!decryptor_->InitializeCDM(key_system.utf8()))
      return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
    current_key_system_ = key_system;
  } else if (key_system != current_key_system_) {
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;
  }

  // TODO(xhwang): We assume all streams are from the same container (thus have
  // the same "type") for now. In the future, the "type" should be passed down
  // from the application.
  if (!decryptor_->GenerateKeyRequest(init_data_type_,
                                      init_data, init_data_length)) {
    current_key_system_.reset();
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
  }

  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::addKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  WebMediaPlayer::MediaKeyException e = AddKeyInternal(
      key_system, key, key_length, init_data, init_data_length, session_id);
  ReportMediaKeyExceptionToUMA("addKey", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::AddKeyInternal(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);
  DVLOG(1) << "addKey: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(key),
                          static_cast<size_t>(key_length)) << ", "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length))
           << " [" << session_id.utf8().data() << "]";

  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  decryptor_->AddKey(key, key_length, init_data, init_data_length,
                     session_id.utf8());
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  WebMediaPlayer::MediaKeyException e =
      CancelKeyRequestInternal(key_system, session_id);
  ReportMediaKeyExceptionToUMA("cancelKeyRequest", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerAndroid::CancelKeyRequestInternal(
    const WebString& key_system,
    const WebString& session_id) {
  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  decryptor_->CancelKeyRequest(session_id.utf8());
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

void WebMediaPlayerAndroid::OnKeyAdded(const std::string& session_id) {
  EmeUMAHistogramCounts(current_key_system_, "KeyAdded", 1);

  if (media_source_delegate_)
    media_source_delegate_->NotifyKeyAdded(current_key_system_.utf8());

  client_->keyAdded(current_key_system_, WebString::fromUTF8(session_id));
}

void WebMediaPlayerAndroid::OnKeyError(const std::string& session_id,
                                       media::MediaKeys::KeyError error_code,
                                       int system_code) {
  EmeUMAHistogramEnumeration(current_key_system_, "KeyError",
                             error_code, media::MediaKeys::kMaxKeyError);

  client_->keyError(
      current_key_system_,
      WebString::fromUTF8(session_id),
      static_cast<WebKit::WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
      system_code);
}

void WebMediaPlayerAndroid::OnKeyMessage(const std::string& session_id,
                                         const std::vector<uint8>& message,
                                         const std::string& destination_url) {
  const GURL destination_url_gurl(destination_url);
  DLOG_IF(WARNING, !destination_url.empty() && !destination_url_gurl.is_valid())
      << "Invalid URL in destination_url: " << destination_url;

  client_->keyMessage(current_key_system_,
                      WebString::fromUTF8(session_id),
                      message.empty() ? NULL : &message[0],
                      message.size(),
                      destination_url_gurl);
}

void WebMediaPlayerAndroid::OnMediaSourceOpened(
    WebKit::WebMediaSourceNew* web_media_source) {
  client_->mediaSourceOpened(web_media_source);
}

void WebMediaPlayerAndroid::OnNeedKey(const std::string& session_id,
                                      const std::string& type,
                                      scoped_ptr<uint8[]> init_data,
                                      int init_data_size) {
  // Do not fire NeedKey event if encrypted media is not enabled.
  if (!WebKit::WebRuntimeFeatures::isEncryptedMediaEnabled() &&
      !WebKit::WebRuntimeFeatures::isLegacyEncryptedMediaEnabled()) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(kMediaEme + std::string("NeedKey"), 1);

  DCHECK(init_data_type_.empty() || type.empty() || type == init_data_type_);
  if (init_data_type_.empty())
    init_data_type_ = type;

  client_->keyNeeded(WebString(),
                     WebString::fromUTF8(session_id),
                     init_data.get(),
                     init_data_size);
}

#if defined(GOOGLE_TV)
bool WebMediaPlayerAndroid::InjectMediaStream(
    MediaStreamClient* media_stream_client,
    media::Demuxer* demuxer,
    const base::Closure& destroy_demuxer_cb) {
  DCHECK(!demuxer);
  media_stream_client_ = media_stream_client;
  demuxer_ = demuxer;
  destroy_demuxer_cb_ = destroy_demuxer_cb;
  return true;
}
#endif

void WebMediaPlayerAndroid::OnReadFromDemuxer(media::DemuxerStream::Type type) {
  if (media_source_delegate_)
    media_source_delegate_->OnReadFromDemuxer(type);
  else
    NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::enterFullscreen() {
  if (manager_->CanEnterFullscreen(frame_)) {
    proxy_->EnterFullscreen(player_id_);
    SetNeedsEstablishPeer(false);
  }
}

void WebMediaPlayerAndroid::exitFullscreen() {
  proxy_->ExitFullscreen(player_id_);
}

bool WebMediaPlayerAndroid::canEnterFullscreen() const {
  return manager_->CanEnterFullscreen(frame_);
}

}  // namespace content
