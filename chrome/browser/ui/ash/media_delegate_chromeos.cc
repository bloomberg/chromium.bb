// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_delegate_chromeos.h"

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"

namespace {

void GetMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    content::WebContents* web_contents,
    int* media_state_out) {
  if (indicator->IsCapturingVideo(web_contents))
    *media_state_out |= ash::MEDIA_CAPTURE_VIDEO;
  if (indicator->IsCapturingAudio(web_contents))
    *media_state_out |= ash::MEDIA_CAPTURE_AUDIO;
}

void GetBrowserMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    const content::BrowserContext* context,
    int* media_state_out) {

  const BrowserList* desktop_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);

  for (BrowserList::BrowserVector::const_iterator iter = desktop_list->begin();
       iter != desktop_list->end();
       ++iter) {
    TabStripModel* tab_strip_model = (*iter)->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (web_contents->GetBrowserContext() != context)
        continue;
      GetMediaCaptureState(indicator, web_contents, media_state_out);
      if (*media_state_out == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
        return;
    }
  }
}

void GetAppMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    content::BrowserContext* context,
    int* media_state_out) {
  const apps::AppWindowRegistry::AppWindowList& apps =
      apps::AppWindowRegistry::Get(context)->app_windows();
  for (apps::AppWindowRegistry::AppWindowList::const_iterator iter =
           apps.begin();
       iter != apps.end();
       ++iter) {
    GetMediaCaptureState(indicator, (*iter)->web_contents(), media_state_out);
    if (*media_state_out == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
      return;
  }
}

void GetExtensionMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    content::BrowserContext* context,
    int* media_state_out) {
  extensions::ProcessManager* process_manager =
      extensions::ExtensionSystem::Get(context)->process_manager();
  const extensions::ProcessManager::ViewSet view_set =
      process_manager->GetAllViews();
  for (extensions::ProcessManager::ViewSet::const_iterator iter =
           view_set.begin();
       iter != view_set.end();
       ++iter) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(*iter);
    // RVH may not have web contents.
    if (!web_contents)
      continue;
    GetMediaCaptureState(indicator, web_contents, media_state_out);
    if (*media_state_out == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
      return;
  }
}

ash::MediaCaptureState GetMediaCaptureStateOfAllWebContents(
    content::BrowserContext* context) {
  if (!context)
    return ash::MEDIA_CAPTURE_NONE;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  int media_state = ash::MEDIA_CAPTURE_NONE;
  // Browser windows
  GetBrowserMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
    return ash::MEDIA_CAPTURE_AUDIO_VIDEO;

  // App windows
  GetAppMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
    return ash::MEDIA_CAPTURE_AUDIO_VIDEO;

  // Extensions
  GetExtensionMediaCaptureState(indicator.get(), context, &media_state);

  return static_cast<ash::MediaCaptureState>(media_state);
}

}  // namespace

MediaDelegateChromeOS::MediaDelegateChromeOS() : weak_ptr_factory_(this) {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
}

MediaDelegateChromeOS::~MediaDelegateChromeOS() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

void MediaDelegateChromeOS::HandleMediaNextTrack() {
  extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile())
      ->media_player_event_router()
      ->NotifyNextTrack();
}

void MediaDelegateChromeOS::HandleMediaPlayPause() {
  extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile())
      ->media_player_event_router()
      ->NotifyTogglePlayState();
}

void MediaDelegateChromeOS::HandleMediaPrevTrack() {
  extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile())
      ->media_player_event_router()
      ->NotifyPrevTrack();
}

ash::MediaCaptureState MediaDelegateChromeOS::GetMediaCaptureState(
    content::BrowserContext* context) {
  return GetMediaCaptureStateOfAllWebContents(context);
}

void MediaDelegateChromeOS::OnRequestUpdate(
    int render_process_id,
    int render_frame_id,
    content::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&MediaDelegateChromeOS::NotifyMediaCaptureChange,
                 weak_ptr_factory_.GetWeakPtr()));
}

void MediaDelegateChromeOS::NotifyMediaCaptureChange() {
  ash::Shell::GetInstance()
      ->system_tray_notifier()
      ->NotifyMediaCaptureChanged();
}
