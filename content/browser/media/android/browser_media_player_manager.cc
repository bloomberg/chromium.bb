// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_media_player_manager.h"

#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/media/android/browser_demuxer_android.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/media/cdm_messages.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/android/external_video_surface_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/android/media_source_player.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_switches.h"

using media::MediaKeys;
using media::MediaPlayerAndroid;
using media::MediaPlayerBridge;
using media::MediaPlayerManager;
using media::MediaSourcePlayer;

namespace content {

// Threshold on the number of media players per renderer before we start
// attempting to release inactive media players.
const int kMediaPlayerThreshold = 1;

// Maximum lengths for various EME API parameters. These are checks to
// prevent unnecessarily large parameters from being passed around, and the
// lengths are somewhat arbitrary as the EME spec doesn't specify any limits.
const size_t kMaxInitDataLength = 64 * 1024;  // 64 KB
const size_t kMaxSessionResponseLength = 64 * 1024;  // 64 KB
const size_t kMaxKeySystemLength = 256;

static BrowserMediaPlayerManager::Factory g_factory = NULL;

// static
void BrowserMediaPlayerManager::RegisterFactory(Factory factory) {
  g_factory = factory;
}

// static
BrowserMediaPlayerManager* BrowserMediaPlayerManager::Create(
    RenderFrameHost* rfh) {
  if (g_factory)
    return g_factory(rfh);
  return new BrowserMediaPlayerManager(rfh);
}

ContentViewCoreImpl* BrowserMediaPlayerManager::GetContentViewCore() const {
  return ContentViewCoreImpl::FromWebContents(web_contents());
}

MediaPlayerAndroid* BrowserMediaPlayerManager::CreateMediaPlayer(
    MediaPlayerHostMsg_Initialize_Type type,
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    int demuxer_client_id,
    bool hide_url_log,
    MediaPlayerManager* manager,
    BrowserDemuxerAndroid* demuxer) {
  switch (type) {
    case MEDIA_PLAYER_TYPE_URL: {
      const std::string user_agent = GetContentClient()->GetUserAgent();
      MediaPlayerBridge* media_player_bridge = new MediaPlayerBridge(
          player_id,
          url,
          first_party_for_cookies,
          user_agent,
          hide_url_log,
          manager,
          base::Bind(&BrowserMediaPlayerManager::OnMediaResourcesRequested,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BrowserMediaPlayerManager::OnMediaResourcesReleased,
                     weak_ptr_factory_.GetWeakPtr()));
      BrowserMediaPlayerManager* browser_media_player_manager =
          static_cast<BrowserMediaPlayerManager*>(manager);
      ContentViewCoreImpl* content_view_core_impl =
          static_cast<ContentViewCoreImpl*>(ContentViewCore::FromWebContents(
              browser_media_player_manager->web_contents_));
      if (!content_view_core_impl) {
        // May reach here due to prerendering. Don't extract the metadata
        // since it is expensive.
        // TODO(qinmin): extract the metadata once the user decided to load
        // the page.
        browser_media_player_manager->OnMediaMetadataChanged(
            player_id, base::TimeDelta(), 0, 0, false);
      } else if (!content_view_core_impl->ShouldBlockMediaRequest(url)) {
        media_player_bridge->Initialize();
      }
      return media_player_bridge;
    }

    case MEDIA_PLAYER_TYPE_MEDIA_SOURCE: {
      return new MediaSourcePlayer(
          player_id,
          manager,
          base::Bind(&BrowserMediaPlayerManager::OnMediaResourcesRequested,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BrowserMediaPlayerManager::OnMediaResourcesReleased,
                     weak_ptr_factory_.GetWeakPtr()),
          demuxer->CreateDemuxer(demuxer_client_id));
    }
  }

  NOTREACHED();
  return NULL;
}

BrowserMediaPlayerManager::BrowserMediaPlayerManager(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host),
      fullscreen_player_id_(-1),
      fullscreen_player_is_released_(false),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      weak_ptr_factory_(this) {
}

BrowserMediaPlayerManager::~BrowserMediaPlayerManager() {}

void BrowserMediaPlayerManager::FullscreenPlayerPlay() {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    if (fullscreen_player_is_released_) {
      video_view_->OpenVideo();
      fullscreen_player_is_released_ = false;
    }
    player->Start();
    Send(new MediaPlayerMsg_DidMediaPlayerPlay(RoutingID(),
                                               fullscreen_player_id_));
  }
}

void BrowserMediaPlayerManager::FullscreenPlayerPause() {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    player->Pause(true);
    Send(new MediaPlayerMsg_DidMediaPlayerPause(RoutingID(),
                                                fullscreen_player_id_));
  }
}

void BrowserMediaPlayerManager::FullscreenPlayerSeek(int msec) {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    // TODO(kbalazs): if |fullscreen_player_is_released_| is true
    // at this point, player->GetCurrentTime() will be wrong until
    // FullscreenPlayerPlay (http://crbug.com/322798).
    OnSeekRequest(fullscreen_player_id_,
                  base::TimeDelta::FromMilliseconds(msec));
  }
}

void BrowserMediaPlayerManager::ExitFullscreen(bool release_media_player) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableOverlayFullscreenVideoSubtitle)) {
    if (WebContentsDelegate* delegate = web_contents_->GetDelegate())
      delegate->ToggleFullscreenModeForTab(web_contents_, false);
    if (RenderWidgetHostViewAndroid* view_android =
        static_cast<RenderWidgetHostViewAndroid*>(
            web_contents_->GetRenderWidgetHostView())) {
      view_android->SetOverlayVideoMode(false);
    }
  }

  Send(
      new MediaPlayerMsg_DidExitFullscreen(RoutingID(), fullscreen_player_id_));
  video_view_.reset();
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  fullscreen_player_id_ = -1;
  if (!player)
    return;
  if (release_media_player)
    ReleaseFullscreenPlayer(player);
  else
    player->SetVideoSurface(gfx::ScopedJavaSurface());
}

void BrowserMediaPlayerManager::OnTimeUpdate(int player_id,
                                             base::TimeDelta current_time) {
  Send(
      new MediaPlayerMsg_MediaTimeUpdate(RoutingID(), player_id, current_time));
}

void BrowserMediaPlayerManager::SetVideoSurface(
    gfx::ScopedJavaSurface surface) {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (!player)
    return;

  bool empty_surface = surface.IsEmpty();
  player->SetVideoSurface(surface.Pass());
  if (empty_surface)
    return;

  Send(new MediaPlayerMsg_DidEnterFullscreen(RoutingID(), player->player_id()));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableOverlayFullscreenVideoSubtitle)) {
    return;
  }
  if (RenderWidgetHostViewAndroid* view_android =
      static_cast<RenderWidgetHostViewAndroid*>(
          web_contents_->GetRenderWidgetHostView())) {
    view_android->SetOverlayVideoMode(true);
  }
  if (WebContentsDelegate* delegate = web_contents_->GetDelegate())
    delegate->ToggleFullscreenModeForTab(web_contents_, true);
}

void BrowserMediaPlayerManager::OnMediaMetadataChanged(
    int player_id, base::TimeDelta duration, int width, int height,
    bool success) {
  Send(new MediaPlayerMsg_MediaMetadataChanged(
      RoutingID(), player_id, duration, width, height, success));
  if (fullscreen_player_id_ == player_id)
    video_view_->UpdateMediaMetadata();
}

void BrowserMediaPlayerManager::OnPlaybackComplete(int player_id) {
  Send(new MediaPlayerMsg_MediaPlaybackCompleted(RoutingID(), player_id));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnPlaybackComplete();
}

void BrowserMediaPlayerManager::OnMediaInterrupted(int player_id) {
  // Tell WebKit that the audio should be paused, then release all resources
  Send(new MediaPlayerMsg_MediaPlayerReleased(RoutingID(), player_id));
  OnReleaseResources(player_id);
}

void BrowserMediaPlayerManager::OnBufferingUpdate(
    int player_id, int percentage) {
  Send(new MediaPlayerMsg_MediaBufferingUpdate(
      RoutingID(), player_id, percentage));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnBufferingUpdate(percentage);
}

void BrowserMediaPlayerManager::OnSeekRequest(
    int player_id,
    const base::TimeDelta& time_to_seek) {
  Send(new MediaPlayerMsg_SeekRequest(RoutingID(), player_id, time_to_seek));
}

void BrowserMediaPlayerManager::PauseVideo() {
  Send(new MediaPlayerMsg_PauseVideo(RoutingID()));
}

void BrowserMediaPlayerManager::OnSeekComplete(
    int player_id,
    const base::TimeDelta& current_time) {
  Send(new MediaPlayerMsg_SeekCompleted(RoutingID(), player_id, current_time));
}

void BrowserMediaPlayerManager::OnError(int player_id, int error) {
  Send(new MediaPlayerMsg_MediaError(RoutingID(), player_id, error));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnMediaPlayerError(error);
}

void BrowserMediaPlayerManager::OnVideoSizeChanged(
    int player_id, int width, int height) {
  Send(new MediaPlayerMsg_MediaVideoSizeChanged(RoutingID(), player_id,
      width, height));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnVideoSizeChanged(width, height);
}

media::MediaResourceGetter*
BrowserMediaPlayerManager::GetMediaResourceGetter() {
  if (!media_resource_getter_.get()) {
    RenderProcessHost* host = web_contents()->GetRenderProcessHost();
    BrowserContext* context = host->GetBrowserContext();
    StoragePartition* partition = host->GetStoragePartition();
    fileapi::FileSystemContext* file_system_context =
        partition ? partition->GetFileSystemContext() : NULL;
    // Eventually this needs to be fixed to pass the correct frame rather
    // than just using the main frame.
    media_resource_getter_.reset(new MediaResourceGetterImpl(
        context,
        file_system_context,
        host->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID()));
  }
  return media_resource_getter_.get();
}

MediaPlayerAndroid* BrowserMediaPlayerManager::GetFullscreenPlayer() {
  return GetPlayer(fullscreen_player_id_);
}

MediaPlayerAndroid* BrowserMediaPlayerManager::GetPlayer(int player_id) {
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id)
      return *it;
  }
  return NULL;
}

MediaKeys* BrowserMediaPlayerManager::GetCdm(int cdm_id) {
  CdmMap::const_iterator iter = cdm_map_.find(cdm_id);
  return (iter == cdm_map_.end()) ? NULL : iter->second;
}

void BrowserMediaPlayerManager::DestroyAllMediaPlayers() {
  players_.clear();
  STLDeleteValues(&cdm_map_);
  if (fullscreen_player_id_ != -1) {
    video_view_.reset();
    fullscreen_player_id_ = -1;
  }
}

void BrowserMediaPlayerManager::RequestFullScreen(int player_id) {
  if (fullscreen_player_id_ == player_id)
    return;

  if (fullscreen_player_id_ != -1) {
    // TODO(qinmin): Determine the correct error code we should report to WMPA.
    OnError(player_id, MediaPlayerAndroid::MEDIA_ERROR_DECODE);
    return;
  }

  // Send an IPC to the render process to request the video element to enter
  // fullscreen. OnEnterFullscreen() will be called later on success.
  // This guarantees the fullscreen video will be rendered correctly.
  // TODO(qinmin): make this flag default on android.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGestureRequirementForMediaFullscreen)) {
    Send(new MediaPlayerMsg_RequestFullscreen(RoutingID(), player_id));
  }
}

// The following 5 functions are EME MediaKeySession events.

void BrowserMediaPlayerManager::OnSessionCreated(
    int cdm_id,
    uint32 session_id,
    const std::string& web_session_id) {
  Send(new CdmMsg_SessionCreated(
      RoutingID(), cdm_id, session_id, web_session_id));
}

void BrowserMediaPlayerManager::OnSessionMessage(
    int cdm_id,
    uint32 session_id,
    const std::vector<uint8>& message,
    const GURL& destination_url) {
  GURL verified_gurl = destination_url;
  if (!verified_gurl.is_valid() && !verified_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage destination_url is invalid : "
                  << destination_url.possibly_invalid_spec();
    verified_gurl = GURL::EmptyGURL();  // Replace invalid destination_url.
  }

  Send(new CdmMsg_SessionMessage(
      RoutingID(), cdm_id, session_id, message, verified_gurl));
}

void BrowserMediaPlayerManager::OnSessionReady(int cdm_id, uint32 session_id) {
  Send(new CdmMsg_SessionReady(RoutingID(), cdm_id, session_id));
}

void BrowserMediaPlayerManager::OnSessionClosed(int cdm_id, uint32 session_id) {
  Send(new CdmMsg_SessionClosed(RoutingID(), cdm_id, session_id));
}

void BrowserMediaPlayerManager::OnSessionError(
    int cdm_id,
    uint32 session_id,
    media::MediaKeys::KeyError error_code,
    uint32 system_code) {
  Send(new CdmMsg_SessionError(
      RoutingID(), cdm_id, session_id, error_code, system_code));
}

#if defined(VIDEO_HOLE)
void BrowserMediaPlayerManager::AttachExternalVideoSurface(int player_id,
                                                           jobject surface) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player) {
    player->SetVideoSurface(
        gfx::ScopedJavaSurface::AcquireExternalSurface(surface));
  }
}

void BrowserMediaPlayerManager::DetachExternalVideoSurface(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SetVideoSurface(gfx::ScopedJavaSurface());
}

void BrowserMediaPlayerManager::OnFrameInfoUpdated() {
  if (external_video_surface_container_)
    external_video_surface_container_->OnFrameInfoUpdated();
}

void BrowserMediaPlayerManager::OnNotifyExternalSurface(
    int player_id, bool is_request, const gfx::RectF& rect) {
  if (!web_contents_)
    return;

  if (is_request) {
    OnRequestExternalSurface(player_id, rect);
  }
  if (external_video_surface_container_) {
    external_video_surface_container_->OnExternalVideoSurfacePositionChanged(
        player_id, rect);
  }
}

void BrowserMediaPlayerManager::OnRequestExternalSurface(
    int player_id, const gfx::RectF& rect) {
  if (!external_video_surface_container_) {
    ContentBrowserClient* client = GetContentClient()->browser();
    external_video_surface_container_.reset(
        client->OverrideCreateExternalVideoSurfaceContainer(web_contents_));
  }
  // It's safe to use base::Unretained(this), because the callbacks will not
  // be called after running ReleaseExternalVideoSurface().
  if (external_video_surface_container_) {
    external_video_surface_container_->RequestExternalVideoSurface(
        player_id,
        base::Bind(&BrowserMediaPlayerManager::AttachExternalVideoSurface,
                   base::Unretained(this)),
        base::Bind(&BrowserMediaPlayerManager::DetachExternalVideoSurface,
                   base::Unretained(this)));
  }
}
#endif  // defined(VIDEO_HOLE)

void BrowserMediaPlayerManager::OnEnterFullscreen(int player_id) {
  DCHECK_EQ(fullscreen_player_id_, -1);
#if defined(VIDEO_HOLE)
  if (external_video_surface_container_)
    external_video_surface_container_->ReleaseExternalVideoSurface(player_id);
#endif  // defined(VIDEO_HOLE)
  if (video_view_.get()) {
    fullscreen_player_id_ = player_id;
    video_view_->OpenVideo();
    return;
  } else if (!ContentVideoView::GetInstance()) {
    // In Android WebView, two ContentViewCores could both try to enter
    // fullscreen video, we just ignore the second one.
    video_view_.reset(new ContentVideoView(this));
    base::android::ScopedJavaLocalRef<jobject> j_content_video_view =
        video_view_->GetJavaObject(base::android::AttachCurrentThread());
    if (!j_content_video_view.is_null()) {
      fullscreen_player_id_ = player_id;
      return;
    }
  }

  // Force the second video to exit fullscreen.
  // TODO(qinmin): There is no need to send DidEnterFullscreen message.
  // However, if we don't send the message, page layers will not be
  // correctly restored. http:crbug.com/367346.
  Send(new MediaPlayerMsg_DidEnterFullscreen(RoutingID(), player_id));
  Send(new MediaPlayerMsg_DidExitFullscreen(RoutingID(), player_id));
  video_view_.reset();
}

void BrowserMediaPlayerManager::OnExitFullscreen(int player_id) {
  if (fullscreen_player_id_ == player_id) {
    MediaPlayerAndroid* player = GetPlayer(player_id);
    if (player)
      player->SetVideoSurface(gfx::ScopedJavaSurface());
    video_view_->OnExitFullscreen();
  }
}

void BrowserMediaPlayerManager::OnInitialize(
    MediaPlayerHostMsg_Initialize_Type type,
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    int demuxer_client_id) {
  DCHECK(type != MEDIA_PLAYER_TYPE_MEDIA_SOURCE || demuxer_client_id > 0)
      << "Media source players must have positive demuxer client IDs: "
      << demuxer_client_id;

  RemovePlayer(player_id);

  RenderProcessHostImpl* host = static_cast<RenderProcessHostImpl*>(
      web_contents()->GetRenderProcessHost());
  MediaPlayerAndroid* player = CreateMediaPlayer(
      type, player_id, url, first_party_for_cookies, demuxer_client_id,
      host->GetBrowserContext()->IsOffTheRecord(), this,
      host->browser_demuxer_android());
  if (!player)
    return;

  AddPlayer(player);
}

void BrowserMediaPlayerManager::OnStart(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (!player)
    return;
  player->Start();
  if (fullscreen_player_id_ == player_id && fullscreen_player_is_released_) {
    video_view_->OpenVideo();
    fullscreen_player_is_released_ = false;
  }
}

void BrowserMediaPlayerManager::OnSeek(
    int player_id,
    const base::TimeDelta& time) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SeekTo(time);
}

void BrowserMediaPlayerManager::OnPause(
    int player_id,
    bool is_media_related_action) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->Pause(is_media_related_action);
}

void BrowserMediaPlayerManager::OnSetVolume(int player_id, double volume) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SetVolume(volume);
}

void BrowserMediaPlayerManager::OnSetPoster(int player_id, const GURL& url) {
  // To be overridden by subclasses.
}

void BrowserMediaPlayerManager::OnReleaseResources(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->Release();
  if (player_id == fullscreen_player_id_)
    fullscreen_player_is_released_ = true;
}

void BrowserMediaPlayerManager::OnDestroyPlayer(int player_id) {
  RemovePlayer(player_id);
  if (fullscreen_player_id_ == player_id)
    fullscreen_player_id_ = -1;
}

void BrowserMediaPlayerManager::OnInitializeCdm(int cdm_id,
                                                const std::string& key_system,
                                                const GURL& security_origin) {
  if (key_system.size() > kMaxKeySystemLength) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetCdm() will return null.
    NOTREACHED() << "Invalid key system: " << key_system;
    return;
  }

  AddCdm(cdm_id, key_system, security_origin);
}

void BrowserMediaPlayerManager::OnCreateSession(
    int cdm_id,
    uint32 session_id,
    CdmHostMsg_CreateSession_ContentType content_type,
    const std::vector<uint8>& init_data) {
  if (init_data.size() > kMaxInitDataLength) {
    LOG(WARNING) << "InitData for ID: " << cdm_id
                 << " too long: " << init_data.size();
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  // Convert the session content type into a MIME type. "audio" and "video"
  // don't matter, so using "video" for the MIME type.
  // Ref:
  // https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-createsession
  std::string mime_type;
  switch (content_type) {
    case CREATE_SESSION_TYPE_WEBM:
      mime_type = "video/webm";
      break;
    case CREATE_SESSION_TYPE_MP4:
      mime_type = "video/mp4";
      break;
    default:
      NOTREACHED();
      return;
  }

  if (CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kDisableInfobarForProtectedMediaIdentifier)) {
    CreateSessionIfPermitted(cdm_id, session_id, mime_type, init_data, true);
    return;
  }

  MediaKeys* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  BrowserContext* context =
      web_contents()->GetRenderProcessHost()->GetBrowserContext();

  std::map<int, GURL>::const_iterator iter =
      cdm_security_origin_map_.find(cdm_id);
  if (iter == cdm_security_origin_map_.end()) {
    NOTREACHED();
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  context->RequestProtectedMediaIdentifierPermission(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      iter->second,
      base::Bind(&BrowserMediaPlayerManager::CreateSessionIfPermitted,
                 weak_ptr_factory_.GetWeakPtr(),
                 cdm_id,
                 session_id,
                 mime_type,
                 init_data));
}

void BrowserMediaPlayerManager::OnUpdateSession(
    int cdm_id,
    uint32 session_id,
    const std::vector<uint8>& response) {
  MediaKeys* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  if (response.size() > kMaxSessionResponseLength) {
    LOG(WARNING) << "Response for ID " << cdm_id
                 << " is too long: " << response.size();
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  cdm->UpdateSession(session_id, &response[0], response.size());

  CdmToPlayerMap::const_iterator iter = cdm_to_player_map_.find(cdm_id);
  if (iter == cdm_to_player_map_.end())
    return;

  int player_id = iter->second;
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->OnKeyAdded();
}

void BrowserMediaPlayerManager::OnReleaseSession(int cdm_id,
                                                 uint32 session_id) {
  MediaKeys* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  cdm->ReleaseSession(session_id);
}

void BrowserMediaPlayerManager::OnDestroyCdm(int cdm_id) {
  MediaKeys* cdm = GetCdm(cdm_id);
  if (!cdm)
    return;

  CancelAllPendingSessionCreations(cdm_id);
  RemoveCdm(cdm_id);
}

void BrowserMediaPlayerManager::CancelAllPendingSessionCreations(int cdm_id) {
  BrowserContext* context =
      web_contents()->GetRenderProcessHost()->GetBrowserContext();
  std::map<int, GURL>::const_iterator iter =
      cdm_security_origin_map_.find(cdm_id);
  if (iter == cdm_security_origin_map_.end())
    return;
  context->CancelProtectedMediaIdentifierPermissionRequests(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      iter->second);
}

void BrowserMediaPlayerManager::AddPlayer(MediaPlayerAndroid* player) {
  DCHECK(!GetPlayer(player->player_id()));
  players_.push_back(player);
}

void BrowserMediaPlayerManager::RemovePlayer(int player_id) {
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    MediaPlayerAndroid* player = *it;
    if (player->player_id() == player_id) {
      players_.erase(it);
      break;
    }
  }

  for (CdmToPlayerMap::iterator it = cdm_to_player_map_.begin();
       it != cdm_to_player_map_.end();
       ++it) {
    if (it->second == player_id) {
      cdm_to_player_map_.erase(it);
      break;
    }
  }
}

scoped_ptr<media::MediaPlayerAndroid> BrowserMediaPlayerManager::SwapPlayer(
      int player_id, media::MediaPlayerAndroid* player) {
  media::MediaPlayerAndroid* previous_player = NULL;
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      previous_player = *it;
      players_.weak_erase(it);
      players_.push_back(player);
      break;
    }
  }
  return scoped_ptr<media::MediaPlayerAndroid>(previous_player);
}

void BrowserMediaPlayerManager::AddCdm(int cdm_id,
                                       const std::string& key_system,
                                       const GURL& security_origin) {
  DCHECK(!GetCdm(cdm_id));
  base::WeakPtr<BrowserMediaPlayerManager> weak_this =
      weak_ptr_factory_.GetWeakPtr();

  scoped_ptr<MediaKeys> cdm(media::CreateBrowserCdm(
      key_system,
      base::Bind(
          &BrowserMediaPlayerManager::OnSessionCreated, weak_this, cdm_id),
      base::Bind(
          &BrowserMediaPlayerManager::OnSessionMessage, weak_this, cdm_id),
      base::Bind(
          &BrowserMediaPlayerManager::OnSessionReady, weak_this, cdm_id),
      base::Bind(
          &BrowserMediaPlayerManager::OnSessionClosed, weak_this, cdm_id),
      base::Bind(
          &BrowserMediaPlayerManager::OnSessionError, weak_this, cdm_id)));

  if (!cdm) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetCdm() will return null.
    DVLOG(1) << "failed to create CDM.";
    return;
  }

  cdm_map_[cdm_id] = cdm.release();
  cdm_security_origin_map_[cdm_id] = security_origin;
}

void BrowserMediaPlayerManager::RemoveCdm(int cdm_id) {
  // TODO(xhwang): Detach CDM from the player it's set to. In prefixed
  // EME implementation the current code is fine because we always destroy the
  // player before we destroy the DrmBridge. This will not always be the case
  // in unprefixed EME implementation.
  CdmMap::iterator iter = cdm_map_.find(cdm_id);
  if (iter != cdm_map_.end()) {
    delete iter->second;
    cdm_map_.erase(iter);
  }
  cdm_to_player_map_.erase(cdm_id);
  cdm_security_origin_map_.erase(cdm_id);
}

void BrowserMediaPlayerManager::OnSetCdm(int player_id, int cdm_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  MediaKeys* cdm = GetCdm(cdm_id);
  if (!cdm || !player) {
    DVLOG(1) << "Cannot set CDM on the specified player.";
    return;
  }

  // TODO(qinmin): add the logic to decide whether we should create the
  // fullscreen surface for EME lv1.
  player->SetCdm(cdm);
  // Do now support setting one CDM on multiple players.

  if (ContainsKey(cdm_to_player_map_, cdm_id)) {
    DVLOG(1) << "CDM is already set on another player.";
    return;
  }

  cdm_to_player_map_[cdm_id] = player_id;
}

int BrowserMediaPlayerManager::RoutingID() {
  return render_frame_host_->GetRoutingID();
}

bool BrowserMediaPlayerManager::Send(IPC::Message* msg) {
  return render_frame_host_->Send(msg);
}

void BrowserMediaPlayerManager::CreateSessionIfPermitted(
    int cdm_id,
    uint32 session_id,
    const std::string& content_type,
    const std::vector<uint8>& init_data,
    bool permitted) {
  if (!permitted) {
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  MediaKeys* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID: " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  // This could fail, in which case a SessionError will be fired.
  cdm->CreateSession(session_id, content_type, &init_data[0], init_data.size());
}

void BrowserMediaPlayerManager::ReleaseFullscreenPlayer(
    MediaPlayerAndroid* player) {
    player->Release();
}

void BrowserMediaPlayerManager::OnMediaResourcesRequested(int player_id) {
  int num_active_player = 0;
  ScopedVector<MediaPlayerAndroid>::iterator it;
  for (it = players_.begin(); it != players_.end(); ++it) {
    if (!(*it)->IsPlayerReady())
      continue;

    // The player is already active, ignore it.
    if ((*it)->player_id() == player_id)
      return;
    else
      num_active_player++;
  }

  // Number of active players are less than the threshold, do nothing.
  if (num_active_player < kMediaPlayerThreshold)
    return;

  for (it = players_.begin(); it != players_.end(); ++it) {
    if ((*it)->IsPlayerReady() && !(*it)->IsPlaying() &&
        fullscreen_player_id_ != (*it)->player_id()) {
      (*it)->Release();
      Send(new MediaPlayerMsg_MediaPlayerReleased(RoutingID(),
                                                  (*it)->player_id()));
    }
  }
}

void BrowserMediaPlayerManager::OnMediaResourcesReleased(int player_id) {
#if defined(VIDEO_HOLE)
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player && player->IsSurfaceInUse())
    return;
  if (external_video_surface_container_)
    external_video_surface_container_->ReleaseExternalVideoSurface(player_id);
#endif  // defined(VIDEO_HOLE)
}

}  // namespace content
