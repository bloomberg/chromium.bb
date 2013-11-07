// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediaplayer_android.h"

#include <limits>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/video_layer.h"
#include "content/public/common/content_client.h"
#include "content/renderer/media/android/proxy_media_keys.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "grit/content_resources.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/image/image.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"

#if defined(GOOGLE_TV)
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/media_stream_client.h"
#endif

static const uint32 kGLTextureExternalOES = 0x8D65;

using blink::WebMediaPlayer;
using blink::WebSize;
using blink::WebString;
using blink::WebTimeRanges;
using blink::WebURL;
using media::MediaPlayerAndroid;
using media::VideoFrame;

namespace {
// Prefix for histograms related to Encrypted Media Extensions.
const char* kMediaEme = "Media.EME.";
}  // namespace

namespace content {

// static
void WebMediaPlayerAndroid::OnReleaseRemotePlaybackTexture(
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    const base::WeakPtr<WebMediaPlayerAndroid>& player,
    uint32 sync_point) {
  main_loop->PostTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerAndroid::DoReleaseRemotePlaybackTexture,
                 player,
                 sync_point));
}

WebMediaPlayerAndroid::WebMediaPlayerAndroid(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    RendererMediaPlayerManager* manager,
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
      pending_seek_(false),
      seeking_(false),
      did_loading_progress_(false),
      manager_(manager),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      remote_playback_texture_id_(0),
      texture_id_(0),
      texture_mailbox_sync_point_(0),
      stream_id_(0),
      is_playing_(false),
      playing_started_(false),
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
      pending_playback_(false),
      player_type_(MEDIA_PLAYER_TYPE_URL),
      current_time_(0),
      is_remote_(false),
      media_log_(media_log),
      weak_factory_(this) {
  DCHECK(manager_);

  DCHECK(main_thread_checker_.CalledOnValidThread());

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
  TryCreateStreamTextureProxyIfNeeded();

  if (blink::WebRuntimeFeatures::isPrefixedEncryptedMediaEnabled()) {
    // TODO(xhwang): Report an error when there is encrypted stream but EME is
    // not enabled. Currently the player just doesn't start and waits for ever.
    decryptor_.reset(new ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
        client,
        frame,
#else
        manager_,
        player_id_,  // TODO(xhwang): Use media_keys_id when MediaKeys are
                     // separated from WebMediaPlayer.
#endif  // defined(ENABLE_PEPPER_CDMS)
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

  if (manager_) {
    manager_->DestroyPlayer(player_id_);
    manager_->UnregisterMediaPlayer(player_id_);
  }

  if (stream_id_)
    stream_texture_factory_->DestroyStreamTexture(texture_id_);

  if (remote_playback_texture_id_) {
    blink::WebGraphicsContext3D* context =
        stream_texture_factory_->Context3d();
    if (context->makeContextCurrent())
      context->deleteTexture(remote_playback_texture_id_);
  }

  if (base::MessageLoop::current())
    base::MessageLoop::current()->RemoveDestructionObserver(this);

  if (player_type_ == MEDIA_PLAYER_TYPE_MEDIA_SOURCE && delegate_)
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
                                 const blink::WebURL& url,
                                 CORSMode cors_mode) {
  switch (load_type) {
    case LoadTypeURL:
      player_type_ = MEDIA_PLAYER_TYPE_URL;
      break;

    case LoadTypeMediaSource:
      player_type_ = MEDIA_PLAYER_TYPE_MEDIA_SOURCE;
      break;

    case LoadTypeMediaStream:
#if defined(GOOGLE_TV)
      player_type_ = MEDIA_PLAYER_TYPE_MEDIA_STREAM;
      break;
#else
      CHECK(false) << "WebMediaPlayerAndroid doesn't support MediaStream on "
                      "this platform";
      return;
#endif
  }

  has_media_metadata_ = false;
  has_media_info_ = false;

  media::SetDecryptorReadyCB set_decryptor_ready_cb;
  if (decryptor_) {  // |decryptor_| can be NULL is EME if not enabled.
    set_decryptor_ready_cb = base::Bind(&ProxyDecryptor::SetDecryptorReadyCB,
                                        base::Unretained(decryptor_.get()));
  }

  int demuxer_client_id = 0;
  if (player_type_ != MEDIA_PLAYER_TYPE_URL) {
    has_media_info_ = true;

    RendererDemuxerAndroid* demuxer =
        RenderThreadImpl::current()->renderer_demuxer();
    demuxer_client_id = demuxer->GetNextDemuxerClientID();

    media_source_delegate_.reset(new MediaSourceDelegate(
        demuxer, demuxer_client_id, media_loop_, media_log_));

    // |media_source_delegate_| is owned, so Unretained() is safe here.
    if (player_type_ == MEDIA_PLAYER_TYPE_MEDIA_SOURCE) {
      media_source_delegate_->InitializeMediaSource(
          base::Bind(&WebMediaPlayerAndroid::OnMediaSourceOpened,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnNeedKey, base::Unretained(this)),
          set_decryptor_ready_cb,
          base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnDurationChanged,
                     weak_factory_.GetWeakPtr()));
    }
#if defined(GOOGLE_TV)
    // TODO(xhwang): Pass set_decryptor_ready_cb in InitializeMediaStream() to
    // enable ClearKey support for Google TV.
    if (player_type_ == MEDIA_PLAYER_TYPE_MEDIA_STREAM) {
      media_source_delegate_->InitializeMediaStream(
          demuxer_,
          base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                     weak_factory_.GetWeakPtr()));
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

  url_ = url;
  GURL first_party_url = frame_->document().firstPartyForCookies();
  manager_->Initialize(
      player_type_, player_id_, url, first_party_url, demuxer_client_id);

  if (manager_->IsInFullscreen(frame_))
    manager_->EnterFullscreen(player_id_);

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
  // Android doesn't start fetching resources until an implementation-defined
  // event (e.g. playback request) occurs. Sets the network state to IDLE
  // if play is not requested yet.
  if (!playing_started_)
    UpdateNetworkState(WebMediaPlayer::NetworkStateIdle);
}

void WebMediaPlayerAndroid::play() {
#if defined(GOOGLE_TV)
  if (hasVideo() && needs_external_surface_ &&
      !manager_->IsInFullscreen(frame_)) {
    DCHECK(!needs_establish_peer_);
    manager_->RequestExternalSurface(player_id_, last_computed_rect_);
  }
  if (audio_renderer_ && paused())
    audio_renderer_->Play();
#endif

  TryCreateStreamTextureProxyIfNeeded();
  if (hasVideo() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

  if (paused())
    manager_->Start(player_id_);
  UpdatePlayingState(true);
  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
  playing_started_ = true;
}

void WebMediaPlayerAndroid::pause() {
  pause(true);
}

void WebMediaPlayerAndroid::pause(bool is_media_related_action) {
#if defined(GOOGLE_TV)
  if (audio_renderer_ && !paused())
    audio_renderer_->Pause();
#endif
  manager_->Pause(player_id_, is_media_related_action);
  UpdatePlayingState(false);
}

void WebMediaPlayerAndroid::seek(double seconds) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << seconds << ")";

  base::TimeDelta new_seek_time = ConvertSecondsToTimestamp(seconds);

  if (seeking_) {
    if (new_seek_time == seek_time_) {
      if (media_source_delegate_) {
        if (!pending_seek_) {
          // If using media source demuxer, only suppress redundant seeks if
          // there is no pending seek. This enforces that any pending seek that
          // results in a demuxer seek is preceded by matching
          // CancelPendingSeek() and StartWaitingForSeek() calls.
          return;
        }
      } else {
        // Suppress all redundant seeks if unrestricted by media source
        // demuxer API.
        pending_seek_ = false;
        return;
      }
    }

    pending_seek_ = true;
    pending_seek_time_ = new_seek_time;

    if (media_source_delegate_)
      media_source_delegate_->CancelPendingSeek(pending_seek_time_);

    // Later, OnSeekComplete will trigger the pending seek.
    return;
  }

  seeking_ = true;
  seek_time_ = new_seek_time;

  if (media_source_delegate_)
    media_source_delegate_->StartWaitingForSeek(seek_time_);

  // Kick off the asynchronous seek!
  manager_->Seek(player_id_, seek_time_);
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
  manager_->SetVolume(player_id_, volume);
}

bool WebMediaPlayerAndroid::hasVideo() const {
  // If we have obtained video size information before, use it.
  if (has_size_info_ && !natural_size_.isEmpty())
    return true;

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
  if (!net::GetMimeTypeFromFile(base::FilePath(url_.path()), &mime))
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
  // If the player is processing a seek, return the seek time.
  // Blink may still query us if updatePlaybackState() occurs while seeking.
  if (seeking()) {
    return pending_seek_ ?
        pending_seek_time_.InSecondsF() : seek_time_.InSecondsF();
  }

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

void WebMediaPlayerAndroid::paint(blink::WebCanvas* canvas,
                                  const blink::WebRect& rect,
                                  unsigned char alpha) {
  NOTIMPLEMENTED();
}

bool WebMediaPlayerAndroid::copyVideoTextureToPlatformTexture(
    blink::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  if (is_remote_ || !texture_id_)
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
  if (player_type_ == MEDIA_PLAYER_TYPE_URL)
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
    const base::TimeDelta& duration, int width, int height, bool success) {
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
    // because OnDurationChanged() handles this.
    if (ready_state_ > WebMediaPlayer::ReadyStateHaveNothing &&
        player_type_ != MEDIA_PLAYER_TYPE_MEDIA_SOURCE) {
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

  // if the loop attribute is set, timeChanged() will update the current time
  // to 0. It will perform a seek to 0. As the requests to the renderer
  // process are sequential, the OnSeekComplete() will only occur
  // once OnPlaybackComplete() is done. As the playback can only be executed
  // upon completion of OnSeekComplete(), the request needs to be saved.
  is_playing_ = false;
  if (seeking_ && seek_time_ == base::TimeDelta())
    pending_playback_ = true;
}

void WebMediaPlayerAndroid::OnBufferingUpdate(int percentage) {
  buffered_[0].end = duration() * percentage / 100;
  did_loading_progress_ = true;
}

void WebMediaPlayerAndroid::OnSeekRequest(const base::TimeDelta& time_to_seek) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  client_->requestSeek(time_to_seek.InSecondsF());
}

void WebMediaPlayerAndroid::OnSeekComplete(
    const base::TimeDelta& current_time) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  seeking_ = false;
  if (pending_seek_) {
    pending_seek_ = false;
    seek(pending_seek_time_.InSecondsF());
    return;
  }

  OnTimeUpdate(current_time);

  UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  client_->timeChanged();

  if (pending_playback_) {
    play();
    pending_playback_ = false;
  }
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
      manager_->RequestExternalSurface(player_id_, last_computed_rect_);
  } else if (stream_texture_factory_ && !stream_id_) {
    // Do deferred stream texture creation finally.
    DoCreateStreamTexture();
    if (paused()) {
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

void WebMediaPlayerAndroid::OnTimeUpdate(const base::TimeDelta& current_time) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  current_time_ = current_time.InSecondsF();
}

void WebMediaPlayerAndroid::OnConnectedToRemoteDevice() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!media_source_delegate_);
  DrawRemotePlaybackIcon();
  is_remote_ = true;
  SetNeedsEstablishPeer(false);
}

void WebMediaPlayerAndroid::OnDisconnectedFromRemoteDevice() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!media_source_delegate_);
  SetNeedsEstablishPeer(true);
  if (!paused())
    EstablishSurfaceTexturePeer();
  is_remote_ = false;
  ReallocateVideoFrame();
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
    manager_->RequestExternalSurface(player_id_, last_computed_rect_);
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

void WebMediaPlayerAndroid::OnDurationChanged(const base::TimeDelta& duration) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  // Only MSE |player_type_| registers this callback.
  DCHECK_EQ(player_type_, MEDIA_PLAYER_TYPE_MEDIA_SOURCE);

  // Cache the new duration value and trust it over any subsequent duration
  // values received in OnMediaMetadataChanged().
  // TODO(wolenetz): Correctly handle durations that MediaSourcePlayer
  // considers unseekable, including kInfiniteDuration().
  // See http://crbug.com/248396
  duration_ = duration;
  ignore_metadata_duration_change_ = true;

  // Notify MediaPlayerClient that duration has changed, if > HAVE_NOTHING.
  if (ready_state_ > WebMediaPlayer::ReadyStateHaveNothing)
    client_->durationChanged();
}

void WebMediaPlayerAndroid::UpdateNetworkState(
    WebMediaPlayer::NetworkState state) {
  DCHECK(main_loop_->BelongsToCurrentThread());
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
      pause(false);
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
  manager_->ReleaseResources(player_id_);
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
  {
    base::AutoLock auto_lock(current_frame_lock_);
    current_frame_ = NULL;
  }
  is_remote_ = false;
  manager_ = NULL;
}

void WebMediaPlayerAndroid::DrawRemotePlaybackIcon() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!video_weblayer_)
    return;
  blink::WebGraphicsContext3D* context = stream_texture_factory_->Context3d();
  if (!context->makeContextCurrent())
    return;

  // TODO(johnme): Should redraw this frame if the layer bounds change; but
  // there seems no easy way to listen for the layer resizing (as opposed to
  // OnVideoSizeChanged, which is when the frame sizes of the video file
  // change). Perhaps have to poll (on main thread of course)?
  gfx::Size video_size_css_px = video_weblayer_->bounds();
  float device_scale_factor = frame_->view()->deviceScaleFactor();
  // canvas_size will be the size in device pixels when pageScaleFactor == 1
  gfx::Size canvas_size(
      static_cast<int>(video_size_css_px.width() * device_scale_factor),
      static_cast<int>(video_size_css_px.height() * device_scale_factor));

  SkBitmap bitmap;
  bitmap.setConfig(
      SkBitmap::kARGB_8888_Config, canvas_size.width(), canvas_size.height());
  bitmap.allocPixels();

  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorBLACK);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterLevel(SkPaint::kHigh_FilterLevel);
  const SkBitmap* icon_bitmap =
      content::GetContentClient()
          ->GetNativeImageNamed(IDR_MEDIAPLAYER_REMOTE_PLAYBACK_ICON)
          .ToSkBitmap();
  // In order to get a reasonable margin around the icon:
  // - the icon should be under half the frame width
  // - the icon should be at most 3/5 of the frame height
  // Additionally, on very large screens, the icon size should be capped. A max
  // width of 320 was arbitrarily chosen; since this is half the resource's
  // pixel width, it should look crisp even on 2x deviceScaleFactor displays.
  int icon_width = 320;
  icon_width = std::min(icon_width, canvas_size.width() / 2);
  icon_width = std::min(icon_width,
                        canvas_size.height() * icon_bitmap->width() /
                            icon_bitmap->height() * 3 / 5);
  int icon_height = icon_width * icon_bitmap->height() / icon_bitmap->width();
  // Center the icon within the frame
  SkRect icon_rect = SkRect::MakeXYWH((canvas_size.width() - icon_width) / 2,
                                      (canvas_size.height() - icon_height) / 2,
                                      icon_width,
                                      icon_height);
  canvas.drawBitmapRectToRect(
      *icon_bitmap, NULL /* src */, icon_rect /* dest */, &paint);

  if (!remote_playback_texture_id_)
    remote_playback_texture_id_ = context->createTexture();
  unsigned texture_target = GL_TEXTURE_2D;
  context->bindTexture(texture_target, remote_playback_texture_id_);
  context->texParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  context->texParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  context->texParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  context->texParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  {
    SkAutoLockPixels lock(bitmap);
    context->texImage2D(texture_target,
                        0 /* level */,
                        GL_RGBA /* internalformat */,
                        bitmap.width(),
                        bitmap.height(),
                        0 /* border */,
                        GL_RGBA /* format */,
                        GL_UNSIGNED_BYTE /* type */,
                        bitmap.getPixels());
  }

  gpu::Mailbox texture_mailbox;
  context->genMailboxCHROMIUM(texture_mailbox.name);
  context->produceTextureCHROMIUM(texture_target, texture_mailbox.name);
  context->flush();
  unsigned texture_mailbox_sync_point = context->insertSyncPoint();

  scoped_refptr<VideoFrame> new_frame = VideoFrame::WrapNativeTexture(
      new VideoFrame::MailboxHolder(
          texture_mailbox,
          texture_mailbox_sync_point,
          base::Bind(&WebMediaPlayerAndroid::OnReleaseRemotePlaybackTexture,
                     main_loop_,
                     weak_factory_.GetWeakPtr())),
      texture_target,
      canvas_size /* coded_size */,
      gfx::Rect(canvas_size) /* visible_rect */,
      canvas_size /* natural_size */,
      base::TimeDelta() /* timestamp */,
      VideoFrame::ReadPixelsCB(),
      base::Closure() /* no_longer_needed_cb */);
  SetCurrentFrameInternal(new_frame);
}

void WebMediaPlayerAndroid::ReallocateVideoFrame() {
  if (needs_external_surface_) {
    // VideoFrame::CreateHoleFrame is only defined under GOOGLE_TV.
#if defined(GOOGLE_TV)
    if (!natural_size_.isEmpty()) {
      scoped_refptr<VideoFrame> new_frame =
          VideoFrame::CreateHoleFrame(natural_size_);
      SetCurrentFrameInternal(new_frame);
      // Force the client to grab the hole frame.
      client_->repaint();
    }
#else
    NOTIMPLEMENTED() << "Hole punching not supported outside of Google TV";
#endif
  } else if (!is_remote_ && texture_id_) {
    scoped_refptr<VideoFrame> new_frame = VideoFrame::WrapNativeTexture(
        new VideoFrame::MailboxHolder(
            texture_mailbox_,
            texture_mailbox_sync_point_,
            VideoFrame::MailboxHolder::TextureNoLongerNeededCallback()),
        kGLTextureExternalOES, natural_size_,
        gfx::Rect(natural_size_), natural_size_, base::TimeDelta(),
        VideoFrame::ReadPixelsCB(),
        base::Closure());
    SetCurrentFrameInternal(new_frame);
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

void WebMediaPlayerAndroid::SetCurrentFrameInternal(
    scoped_refptr<media::VideoFrame>& video_frame) {
  base::AutoLock auto_lock(current_frame_lock_);
  current_frame_ = video_frame;
}

scoped_refptr<media::VideoFrame> WebMediaPlayerAndroid::GetCurrentFrame() {
  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(current_frame_lock_);
    video_frame = current_frame_;
  }

  if (!stream_texture_proxy_initialized_ && stream_texture_proxy_ &&
      stream_id_ && !needs_external_surface_ && !is_remote_) {
    gfx::Size natural_size = video_frame->natural_size();
    // TODO(sievers): These variables are accessed on the wrong thread here.
    stream_texture_proxy_->BindToCurrentThread(stream_id_);
    stream_texture_factory_->SetStreamTextureSize(stream_id_, natural_size);
    stream_texture_proxy_initialized_ = true;
    cached_stream_texture_size_ = natural_size;
  }

  return video_frame;
}

void WebMediaPlayerAndroid::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

void WebMediaPlayerAndroid::TryCreateStreamTextureProxyIfNeeded() {
  // Already created.
  if (stream_texture_proxy_)
    return;

  // No factory to create proxy.
  if (!stream_texture_factory_)
    return;

  stream_texture_proxy_.reset(stream_texture_factory_->CreateProxy());
  if (needs_establish_peer_ && stream_texture_proxy_) {
    DoCreateStreamTexture();
    ReallocateVideoFrame();
  }

  if (stream_texture_proxy_ && video_frame_provider_client_)
    stream_texture_proxy_->SetClient(video_frame_provider_client_);
}

void WebMediaPlayerAndroid::EstablishSurfaceTexturePeer() {
  if (!stream_texture_proxy_)
    return;

  if (media_source_delegate_ && stream_texture_factory_) {
    // MediaCodec will release the old surface when it goes away, we need to
    // recreate a new one each time this is called.
    stream_texture_factory_->DestroyStreamTexture(texture_id_);
    stream_id_ = 0;
    texture_id_ = 0;
    texture_mailbox_ = gpu::Mailbox();
    texture_mailbox_sync_point_ = 0;
    DoCreateStreamTexture();
    ReallocateVideoFrame();
    stream_texture_proxy_initialized_ = false;
  }
  if (stream_texture_factory_.get() && stream_id_)
    stream_texture_factory_->EstablishPeer(stream_id_, player_id_);
  needs_establish_peer_ = false;
}

void WebMediaPlayerAndroid::DoCreateStreamTexture() {
  DCHECK(!stream_id_);
  DCHECK(!texture_id_);
  DCHECK(!texture_mailbox_sync_point_);
  stream_id_ = stream_texture_factory_->CreateStreamTexture(
      kGLTextureExternalOES,
      &texture_id_,
      &texture_mailbox_,
      &texture_mailbox_sync_point_);
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
static void EmeUMAHistogramEnumeration(const blink::WebString& key_system,
                                       const std::string& method,
                                       int sample,
                                       int boundary_value) {
  base::LinearHistogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, boundary_value, boundary_value + 1,
      base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

static void EmeUMAHistogramCounts(const blink::WebString& key_system,
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

  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.isEmpty()) {
    if (!decryptor_->InitializeCDM(key_system.utf8(), frame_->document().url()))
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

  if (!IsConcreteSupportedKeySystem(key_system))
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
  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  decryptor_->CancelKeyRequest(session_id.utf8());
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

void WebMediaPlayerAndroid::OnKeyAdded(const std::string& session_id) {
  EmeUMAHistogramCounts(current_key_system_, "KeyAdded", 1);

#if defined(GOOGLE_TV)
  if (media_source_delegate_)
    media_source_delegate_->NotifyKeyAdded(current_key_system_.utf8());
#endif  // defined(GOOGLE_TV)

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
      static_cast<blink::WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
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
    blink::WebMediaSource* web_media_source) {
  client_->mediaSourceOpened(web_media_source);
}

void WebMediaPlayerAndroid::OnNeedKey(const std::string& type,
                                      const std::vector<uint8>& init_data) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  // Do not fire NeedKey event if encrypted media is not enabled.
  if (!blink::WebRuntimeFeatures::isEncryptedMediaEnabled() &&
      !blink::WebRuntimeFeatures::isPrefixedEncryptedMediaEnabled()) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(kMediaEme + std::string("NeedKey"), 1);

  DCHECK(init_data_type_.empty() || type.empty() || type == init_data_type_);
  if (init_data_type_.empty())
    init_data_type_ = type;

  const uint8* init_data_ptr = init_data.empty() ? NULL : &init_data[0];
  // TODO(xhwang): Drop |keySystem| and |sessionId| in keyNeeded() call.
  client_->keyNeeded(WebString(),
                     WebString(),
                     init_data_ptr,
                     init_data.size());
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

void WebMediaPlayerAndroid::DoReleaseRemotePlaybackTexture(uint32 sync_point) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(remote_playback_texture_id_);

  blink::WebGraphicsContext3D* context =
      stream_texture_factory_->Context3d();

  if (sync_point)
    context->waitSyncPoint(sync_point);
  context->deleteTexture(remote_playback_texture_id_);
  remote_playback_texture_id_ = 0;
}

void WebMediaPlayerAndroid::enterFullscreen() {
  if (manager_->CanEnterFullscreen(frame_)) {
    manager_->EnterFullscreen(player_id_);
    SetNeedsEstablishPeer(false);
  }
}

void WebMediaPlayerAndroid::exitFullscreen() {
  manager_->ExitFullscreen(player_id_);
}

bool WebMediaPlayerAndroid::canEnterFullscreen() const {
  return manager_->CanEnterFullscreen(frame_);
}

}  // namespace content
