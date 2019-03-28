// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_client.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

using ash::mojom::MediaCaptureState;

namespace {

MediaClient* g_media_client_ = nullptr;

MediaCaptureState& operator|=(MediaCaptureState& lhs, MediaCaptureState rhs) {
  lhs = static_cast<MediaCaptureState>(static_cast<int>(lhs) |
                                       static_cast<int>(rhs));
  return lhs;
}

void GetMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                          content::WebContents* web_contents,
                          MediaCaptureState* media_state_out) {
  bool video = indicator->IsCapturingVideo(web_contents);
  bool audio = indicator->IsCapturingAudio(web_contents);

  if (video)
    *media_state_out |= MediaCaptureState::VIDEO;
  if (audio)
    *media_state_out |= MediaCaptureState::AUDIO;
}

void GetBrowserMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                                 const content::BrowserContext* context,
                                 MediaCaptureState* media_state_out) {
  const BrowserList* desktop_list = BrowserList::GetInstance();

  for (BrowserList::BrowserVector::const_iterator iter = desktop_list->begin();
       iter != desktop_list->end(); ++iter) {
    TabStripModel* tab_strip_model = (*iter)->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (web_contents->GetBrowserContext() != context)
        continue;
      GetMediaCaptureState(indicator, web_contents, media_state_out);
      if (*media_state_out == MediaCaptureState::AUDIO_VIDEO)
        return;
    }
  }
}

void GetAppMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                             content::BrowserContext* context,
                             MediaCaptureState* media_state_out) {
  const extensions::AppWindowRegistry::AppWindowList& apps =
      extensions::AppWindowRegistry::Get(context)->app_windows();
  for (extensions::AppWindowRegistry::AppWindowList::const_iterator iter =
           apps.begin();
       iter != apps.end(); ++iter) {
    GetMediaCaptureState(indicator, (*iter)->web_contents(), media_state_out);
    if (*media_state_out == MediaCaptureState::AUDIO_VIDEO)
      return;
  }
}

void GetExtensionMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                                   content::BrowserContext* context,
                                   MediaCaptureState* media_state_out) {
  for (content::RenderFrameHost* host :
       extensions::ProcessManager::Get(context)->GetAllFrames()) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(host);
    // RFH may not have web contents.
    if (!web_contents)
      continue;
    GetMediaCaptureState(indicator, web_contents, media_state_out);
    if (*media_state_out == MediaCaptureState::AUDIO_VIDEO)
      return;
  }
}

MediaCaptureState GetMediaCaptureStateOfAllWebContents(
    content::BrowserContext* context) {
  if (!context)
    return MediaCaptureState::NONE;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  MediaCaptureState media_state = MediaCaptureState::NONE;
  // Browser windows
  GetBrowserMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == MediaCaptureState::AUDIO_VIDEO)
    return MediaCaptureState::AUDIO_VIDEO;

  // App windows
  GetAppMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == MediaCaptureState::AUDIO_VIDEO)
    return MediaCaptureState::AUDIO_VIDEO;

  // Extensions
  GetExtensionMediaCaptureState(indicator.get(), context, &media_state);

  return media_state;
}

}  // namespace

MediaClient::MediaClient() {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  BrowserList::GetInstance()->AddObserver(this);

  DCHECK(!g_media_client_);
  g_media_client_ = this;
}

MediaClient::~MediaClient() {
  g_media_client_ = nullptr;

  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
  BrowserList::GetInstance()->RemoveObserver(this);
}

// static
MediaClient* MediaClient::Get() {
  return g_media_client_;
}

void MediaClient::Init() {
  DCHECK(!media_controller_.is_bound());

  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(ash::mojom::kServiceName, &media_controller_);
  BindAndSetClient();
}

void MediaClient::InitForTesting(ash::mojom::MediaControllerPtr controller) {
  DCHECK(!media_controller_.is_bound());

  media_controller_ = std::move(controller);
  BindAndSetClient();
}

void MediaClient::HandleMediaNextTrack() {
  HandleMediaAction(ui::VKEY_MEDIA_NEXT_TRACK);
}

void MediaClient::HandleMediaPlayPause() {
  HandleMediaAction(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaClient::HandleMediaPrevTrack() {
  HandleMediaAction(ui::VKEY_MEDIA_PREV_TRACK);
}

void MediaClient::RequestCaptureState() {
  base::flat_map<AccountId, MediaCaptureState> capture_states;
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetLRULoggedInUsers()) {
    capture_states[user->GetAccountId()] = GetMediaCaptureStateOfAllWebContents(
        chromeos::ProfileHelper::Get()->GetProfileByUser(user));
  }

  media_controller_->NotifyCaptureState(std::move(capture_states));
}

void MediaClient::SuspendMediaSessions() {
  for (auto* web_contents : AllTabContentses()) {
    content::MediaSession::Get(web_contents)
        ->Suspend(content::MediaSession::SuspendType::kSystem);
  }
}

void MediaClient::OnRequestUpdate(int render_process_id,
                                  int render_frame_id,
                                  blink::MediaStreamType stream_type,
                                  const content::MediaRequestState state) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  // The PostTask is necessary because the state of MediaStreamCaptureIndicator
  // gets updated after this.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MediaClient::RequestCaptureState,
                                weak_ptr_factory_.GetWeakPtr()));
}

void MediaClient::OnBrowserSetLastActive(Browser* browser) {
  active_context_ = browser ? browser->profile() : nullptr;

  UpdateForceMediaClientKeyHandling();
}

void MediaClient::EnableCustomMediaKeyHandler(
    content::BrowserContext* context,
    ui::MediaKeysListener::Delegate* delegate) {
  auto it = media_key_delegates_.find(context);

  DCHECK(!base::ContainsKey(media_key_delegates_, context) ||
         it->second == delegate);

  media_key_delegates_.emplace(context, delegate);

  UpdateForceMediaClientKeyHandling();
}

void MediaClient::DisableCustomMediaKeyHandler(
    content::BrowserContext* context,
    ui::MediaKeysListener::Delegate* delegate) {
  if (!base::ContainsKey(media_key_delegates_, context))
    return;

  auto it = media_key_delegates_.find(context);
  DCHECK_EQ(it->second, delegate);

  media_key_delegates_.erase(it);

  UpdateForceMediaClientKeyHandling();
}

void MediaClient::FlushForTesting() {
  media_controller_.FlushForTesting();
}

void MediaClient::BindAndSetClient() {
  ash::mojom::MediaClientAssociatedPtrInfo ptr_info;
  binding_.Bind(mojo::MakeRequest(&ptr_info));
  media_controller_->SetClient(std::move(ptr_info));
}

void MediaClient::UpdateForceMediaClientKeyHandling() {
  bool enabled = GetCurrentMediaKeyDelegate() != nullptr;

  if (enabled == is_forcing_media_client_key_handling_)
    return;

  is_forcing_media_client_key_handling_ = enabled;

  media_controller_->SetForceMediaClientKeyHandling(enabled);
}

ui::MediaKeysListener::Delegate* MediaClient::GetCurrentMediaKeyDelegate()
    const {
  auto it = media_key_delegates_.find(active_context_);
  if (it != media_key_delegates_.end())
    return it->second;

  return nullptr;
}

void MediaClient::HandleMediaAction(ui::KeyboardCode keycode) {
  if (ui::MediaKeysListener::Delegate* custom = GetCurrentMediaKeyDelegate()) {
    custom->OnMediaKeysAccelerator(ui::Accelerator(keycode, ui::EF_NONE));
    return;
  }

  // This API is used by Chrome apps so we should use the logged in user here.
  extensions::MediaPlayerAPI* player_api =
      extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile());
  if (!player_api)
    return;

  extensions::MediaPlayerEventRouter* router =
      player_api->media_player_event_router();
  if (!router)
    return;

  switch (keycode) {
    case ui::VKEY_MEDIA_NEXT_TRACK:
      router->NotifyNextTrack();
      break;
    case ui::VKEY_MEDIA_PREV_TRACK:
      router->NotifyPrevTrack();
      break;
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      router->NotifyTogglePlayState();
      break;
    default:
      break;
  }
}
