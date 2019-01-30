// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_media_player_manager.h"

#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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
#include "media/base/android/media_url_interceptor.h"
#include "media/base/media_content_type.h"

#if !defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#endif

using media::MediaPlayerAndroid;
using media::MediaPlayerBridge;
using media::MediaPlayerManager;

namespace content {

// static
void BrowserMediaPlayerManager::RegisterFactory(Factory factory) {
  NOTREACHED();
}

// static
void BrowserMediaPlayerManager::RegisterMediaUrlInterceptor(
    media::MediaUrlInterceptor* media_url_interceptor) {
  // NOTREACHED() here causes some crashes in some test suites.
  // This entire class will be removed very shortly in a follow up patch,
  // so NO-OP is fine for now.
}

// static
BrowserMediaPlayerManager* BrowserMediaPlayerManager::Create(
    RenderFrameHost* rfh) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<MediaPlayerAndroid>
BrowserMediaPlayerManager::CreateMediaPlayer(
    const MediaPlayerHostMsg_Initialize_Params& media_player_params,
    bool hide_url_log) {
  NOTREACHED();
  return nullptr;
}

BrowserMediaPlayerManager::BrowserMediaPlayerManager(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      weak_ptr_factory_(this) {
}

BrowserMediaPlayerManager::~BrowserMediaPlayerManager() {
}

void BrowserMediaPlayerManager::OnTimeUpdate(
    int player_id,
    base::TimeDelta current_timestamp,
    base::TimeTicks current_time_ticks) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnMediaMetadataChanged(
    int player_id, base::TimeDelta duration, int width, int height,
    bool success) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnPlaybackComplete(int player_id) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnMediaInterrupted(int player_id) {
  // Tell WebKit that the audio should be paused, then release all resources
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnBufferingUpdate(int player_id,
                                                  int percentage) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnSeekRequest(
    int player_id,
    const base::TimeDelta& time_to_seek) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnSeekComplete(
    int player_id,
    const base::TimeDelta& current_time) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnError(int player_id, int error) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnVideoSizeChanged(
    int player_id, int width, int height) {
  NOTREACHED();
}

media::MediaResourceGetter*
BrowserMediaPlayerManager::GetMediaResourceGetter() {
  NOTREACHED();
  return nullptr;
}

media::MediaUrlInterceptor*
BrowserMediaPlayerManager::GetMediaUrlInterceptor() {
  NOTREACHED();
  return nullptr;
}

MediaPlayerAndroid* BrowserMediaPlayerManager::GetPlayer(int player_id) {
  NOTREACHED();
  return nullptr;
}

bool BrowserMediaPlayerManager::RequestPlay(int player_id,
                                            base::TimeDelta duration,
                                            bool has_audio) {
  NOTREACHED();
  return false;
}

void BrowserMediaPlayerManager::OnInitialize(
    const MediaPlayerHostMsg_Initialize_Params& media_player_params) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnStart(int player_id) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnSeek(
    int player_id,
    const base::TimeDelta& time) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnPause(
    int player_id,
    bool is_media_related_action) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnSetVolume(int player_id, double volume) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnSetPoster(int player_id, const GURL& url) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnSuspendAndReleaseResources(int player_id) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnDestroyPlayer(int player_id) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnRequestRemotePlayback(int /* player_id */) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnRequestRemotePlaybackControl(
    int /* player_id */) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::OnRequestRemotePlaybackStop(
    int /* player_id */) {
  NOTREACHED();
}

bool BrowserMediaPlayerManager::IsPlayingRemotely(int player_id) {
  NOTREACHED();
  return false;
}

void BrowserMediaPlayerManager::AddPlayer(
    std::unique_ptr<MediaPlayerAndroid> player,
    int delegate_id) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::DestroyPlayer(int player_id) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::ReleaseResources(int player_id) {
  NOTREACHED();
}

std::unique_ptr<MediaPlayerAndroid> BrowserMediaPlayerManager::SwapPlayer(
    int player_id,
    std::unique_ptr<MediaPlayerAndroid> player) {
  NOTREACHED();
  return nullptr;
}

bool BrowserMediaPlayerManager::RequestDecoderResources(
    int player_id, bool temporary) {
  NOTREACHED();
  return true;
}

void BrowserMediaPlayerManager::OnDecoderResourcesReleased(int player_id) {
  NOTREACHED();
}

int BrowserMediaPlayerManager::RoutingID() {
  NOTREACHED();
  return 0;
}

bool BrowserMediaPlayerManager::Send(IPC::Message* msg) {
  NOTREACHED();
  return render_frame_host_->Send(msg);
}

void BrowserMediaPlayerManager::ReleaseFullscreenPlayer(
    MediaPlayerAndroid* player) {
  NOTREACHED();
}

void BrowserMediaPlayerManager::ReleasePlayer(MediaPlayerAndroid* player) {
  NOTREACHED();
}

}  // namespace content
