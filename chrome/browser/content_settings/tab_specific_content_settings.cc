// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/tab_specific_content_settings.h"

#include <list>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/cookies/canonical_cookie.h"
#include "storage/common/fileapi/file_system_types.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::RenderViewHost;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TabSpecificContentSettings);
STATIC_CONST_MEMBER_DEFINITION const
    TabSpecificContentSettings::MicrophoneCameraState
    TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;
STATIC_CONST_MEMBER_DEFINITION const
    TabSpecificContentSettings::MicrophoneCameraState
    TabSpecificContentSettings::MICROPHONE_ACCESSED;
STATIC_CONST_MEMBER_DEFINITION const
    TabSpecificContentSettings::MicrophoneCameraState
    TabSpecificContentSettings::MICROPHONE_BLOCKED;
STATIC_CONST_MEMBER_DEFINITION const
    TabSpecificContentSettings::MicrophoneCameraState
    TabSpecificContentSettings::CAMERA_ACCESSED;
STATIC_CONST_MEMBER_DEFINITION const
    TabSpecificContentSettings::MicrophoneCameraState
    TabSpecificContentSettings::CAMERA_BLOCKED;

TabSpecificContentSettings::SiteDataObserver::SiteDataObserver(
    TabSpecificContentSettings* tab_specific_content_settings)
    : tab_specific_content_settings_(tab_specific_content_settings) {
  tab_specific_content_settings_->AddSiteDataObserver(this);
}

TabSpecificContentSettings::SiteDataObserver::~SiteDataObserver() {
  if (tab_specific_content_settings_)
    tab_specific_content_settings_->RemoveSiteDataObserver(this);
}

void TabSpecificContentSettings::SiteDataObserver::ContentSettingsDestroyed() {
  tab_specific_content_settings_ = NULL;
}

TabSpecificContentSettings::TabSpecificContentSettings(WebContents* tab)
    : content::WebContentsObserver(tab),
      profile_(Profile::FromBrowserContext(tab->GetBrowserContext())),
      allowed_local_shared_objects_(profile_),
      blocked_local_shared_objects_(profile_),
      geolocation_usages_state_(profile_, CONTENT_SETTINGS_TYPE_GEOLOCATION),
      midi_usages_state_(profile_, CONTENT_SETTINGS_TYPE_MIDI_SYSEX),
      pending_protocol_handler_(ProtocolHandler::EmptyProtocolHandler()),
      previous_protocol_handler_(ProtocolHandler::EmptyProtocolHandler()),
      pending_protocol_handler_setting_(CONTENT_SETTING_DEFAULT),
      load_plugins_link_enabled_(true),
      microphone_camera_state_(MICROPHONE_CAMERA_NOT_ACCESSED),
      observer_(this) {
  ClearBlockedContentSettingsExceptForCookies();
  ClearCookieSpecificContentSettings();

  observer_.Add(profile_->GetHostContentSettingsMap());
}

TabSpecificContentSettings::~TabSpecificContentSettings() {
  FOR_EACH_OBSERVER(
      SiteDataObserver, observer_list_, ContentSettingsDestroyed());
}

TabSpecificContentSettings* TabSpecificContentSettings::Get(
    int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderViewHost* view = RenderViewHost::FromID(render_process_id,
                                                render_view_id);
  if (!view)
    return NULL;

  WebContents* web_contents = WebContents::FromRenderViewHost(view);
  if (!web_contents)
    return NULL;

  return TabSpecificContentSettings::FromWebContents(web_contents);
}

TabSpecificContentSettings* TabSpecificContentSettings::GetForFrame(
    int render_process_id, int render_frame_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      render_process_id, render_frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(frame);
  if (!web_contents)
    return NULL;

  return TabSpecificContentSettings::FromWebContents(web_contents);
}

// static
void TabSpecificContentSettings::CookiesRead(int render_process_id,
                                             int render_frame_id,
                                             const GURL& url,
                                             const GURL& frame_url,
                                             const net::CookieList& cookie_list,
                                             bool blocked_by_policy,
                                             bool is_for_blocking_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabSpecificContentSettings* settings =
      GetForFrame(render_process_id, render_frame_id);
  if (settings) {
    settings->OnCookiesRead(url, frame_url, cookie_list,
                            blocked_by_policy);
  }
  prerender::PrerenderManager::RecordCookieEvent(
      render_process_id,
      render_frame_id,
      url,
      frame_url,
      is_for_blocking_resource,
      prerender::PrerenderContents::COOKIE_EVENT_SEND,
      &cookie_list);
}

// static
void TabSpecificContentSettings::CookieChanged(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    const GURL& frame_url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    bool blocked_by_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabSpecificContentSettings* settings =
      GetForFrame(render_process_id, render_frame_id);
  if (settings)
    settings->OnCookieChanged(url, frame_url, cookie_line, options,
                              blocked_by_policy);
  prerender::PrerenderManager::RecordCookieEvent(
      render_process_id,
      render_frame_id,
      url,
      frame_url,
      false /*is_critical_request*/,
      prerender::PrerenderContents::COOKIE_EVENT_CHANGE,
      NULL);
}

// static
void TabSpecificContentSettings::WebDatabaseAccessed(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    bool blocked_by_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabSpecificContentSettings* settings = GetForFrame(
      render_process_id, render_frame_id);
  if (settings)
    settings->OnWebDatabaseAccessed(url, name, display_name, blocked_by_policy);
}

// static
void TabSpecificContentSettings::DOMStorageAccessed(int render_process_id,
                                                    int render_frame_id,
                                                    const GURL& url,
                                                    bool local,
                                                    bool blocked_by_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabSpecificContentSettings* settings = GetForFrame(
      render_process_id, render_frame_id);
  if (settings)
    settings->OnLocalStorageAccessed(url, local, blocked_by_policy);
}

// static
void TabSpecificContentSettings::IndexedDBAccessed(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    const base::string16& description,
    bool blocked_by_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabSpecificContentSettings* settings = GetForFrame(
      render_process_id, render_frame_id);
  if (settings)
    settings->OnIndexedDBAccessed(url, description, blocked_by_policy);
}

// static
void TabSpecificContentSettings::FileSystemAccessed(int render_process_id,
                                                    int render_frame_id,
                                                    const GURL& url,
                                                    bool blocked_by_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabSpecificContentSettings* settings = GetForFrame(
      render_process_id, render_frame_id);
  if (settings)
    settings->OnFileSystemAccessed(url, blocked_by_policy);
}

bool TabSpecificContentSettings::IsContentBlocked(
    ContentSettingsType content_type) const {
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by ContentSettingGeolocationImageModel";
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
      << "Notifications settings handled by "
      << "ContentSettingsNotificationsImageModel";

  if (content_type == CONTENT_SETTINGS_TYPE_IMAGES ||
      content_type == CONTENT_SETTINGS_TYPE_JAVASCRIPT ||
      content_type == CONTENT_SETTINGS_TYPE_PLUGINS ||
      content_type == CONTENT_SETTINGS_TYPE_COOKIES ||
      content_type == CONTENT_SETTINGS_TYPE_POPUPS ||
      content_type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT ||
      content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM ||
      content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA ||
      content_type == CONTENT_SETTINGS_TYPE_PPAPI_BROKER ||
      content_type == CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS ||
      content_type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    return content_blocked_[content_type];
  }

  return false;
}

bool TabSpecificContentSettings::IsBlockageIndicated(
    ContentSettingsType content_type) const {
  return content_blockage_indicated_to_user_[content_type];
}

void TabSpecificContentSettings::SetBlockageHasBeenIndicated(
    ContentSettingsType content_type) {
  content_blockage_indicated_to_user_[content_type] = true;
}

bool TabSpecificContentSettings::IsContentAllowed(
    ContentSettingsType content_type) const {
  // This method currently only returns meaningful values for the content type
  // cookies, mediastream, PPAPI broker, and downloads.
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES &&
      content_type != CONTENT_SETTINGS_TYPE_MEDIASTREAM &&
      content_type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC &&
      content_type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA &&
      content_type != CONTENT_SETTINGS_TYPE_PPAPI_BROKER &&
      content_type != CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS &&
      content_type != CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    return false;
  }

  return content_allowed_[content_type];
}

void TabSpecificContentSettings::OnContentBlocked(ContentSettingsType type) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  DCHECK(type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC &&
         type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
      << "Media stream settings handled by OnMediaStreamPermissionSet";
  if (type < 0 || type >= CONTENT_SETTINGS_NUM_TYPES)
    return;

  // TODO(robwu): Should this be restricted to cookies only?
  // In the past, content_allowed_ was set to false, but this logic was inverted
  // in https://codereview.chromium.org/13375004 to fix an issue with the cookie
  // permission UI. This unconditional assignment seems incorrect, because the
  // flag will now always be true after calling either OnContentBlocked or
  // OnContentAllowed. Consequently IsContentAllowed will always return true
  // for every supported setting that is not handled elsewhere.
  content_allowed_[type] = true;

#if defined(OS_ANDROID)
  if (type == CONTENT_SETTINGS_TYPE_POPUPS) {
    // For Android we do not have a persistent button that will always be
    // visible for blocked popups.  Instead we have info bars which could be
    // dismissed.  Have to clear the blocked state so we properly notify the
    // relevant pieces again.
    content_blocked_[type] = false;
    content_blockage_indicated_to_user_[type] = false;
  }
#endif

  if (!content_blocked_[type]) {
    content_blocked_[type] = true;
    // TODO: it would be nice to have a way of mocking this in tests.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
        content::Source<WebContents>(web_contents()),
        content::NotificationService::NoDetails());
  }
}

void TabSpecificContentSettings::OnContentAllowed(ContentSettingsType type) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  DCHECK(type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC &&
         type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
      << "Media stream settings handled by OnMediaStreamPermissionSet";
  bool access_changed = false;
#if defined(OS_ANDROID)
  if (type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER &&
    content_blocked_[type]) {
    // content_allowed_[type] is always set to true in OnContentBlocked, so we
    // have to use content_blocked_ to detect whether the protected media
    // setting has changed.
    content_blocked_[type] = false;
    access_changed = true;
  }
#endif

  if (!content_allowed_[type]) {
    content_allowed_[type] = true;
    access_changed = true;
  }

  if (access_changed) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
        content::Source<WebContents>(web_contents()),
        content::NotificationService::NoDetails());
  }
}

void TabSpecificContentSettings::OnCookiesRead(
    const GURL& url,
    const GURL& frame_url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  if (cookie_list.empty())
    return;
  if (blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->AddReadCookies(
        frame_url, url, cookie_list);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.cookies()->AddReadCookies(
        frame_url, url, cookie_list);
    OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);
  }

  NotifySiteDataObservers();
}

void TabSpecificContentSettings::OnCookieChanged(
    const GURL& url,
    const GURL& frame_url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->AddChangedCookie(
        frame_url, url, cookie_line, options);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.cookies()->AddChangedCookie(
        frame_url, url, cookie_line, options);
    OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);
  }

  NotifySiteDataObservers();
}

void TabSpecificContentSettings::OnIndexedDBAccessed(
    const GURL& url,
    const base::string16& description,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.indexed_dbs()->AddIndexedDB(
        url, description);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.indexed_dbs()->AddIndexedDB(
        url, description);
    OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);
  }

  NotifySiteDataObservers();
}

void TabSpecificContentSettings::OnLocalStorageAccessed(
    const GURL& url,
    bool local,
    bool blocked_by_policy) {
  LocalSharedObjectsContainer& container = blocked_by_policy ?
      blocked_local_shared_objects_ : allowed_local_shared_objects_;
  CannedBrowsingDataLocalStorageHelper* helper =
      local ? container.local_storages() : container.session_storages();
  helper->AddLocalStorage(url);

  if (blocked_by_policy)
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  else
    OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);

  NotifySiteDataObservers();
}

void TabSpecificContentSettings::OnWebDatabaseAccessed(
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.databases()->AddDatabase(
        url, base::UTF16ToUTF8(name), base::UTF16ToUTF8(display_name));
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.databases()->AddDatabase(
        url, base::UTF16ToUTF8(name), base::UTF16ToUTF8(display_name));
    OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);
  }

  NotifySiteDataObservers();
}

void TabSpecificContentSettings::OnFileSystemAccessed(
    const GURL& url,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.file_systems()->AddFileSystem(
        url, storage::kFileSystemTypeTemporary, 0);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.file_systems()->AddFileSystem(
        url, storage::kFileSystemTypeTemporary, 0);
    OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);
  }

  NotifySiteDataObservers();
}

void TabSpecificContentSettings::OnGeolocationPermissionSet(
    const GURL& requesting_origin,
    bool allowed) {
  geolocation_usages_state_.OnPermissionSet(requesting_origin, allowed);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

#if defined(OS_ANDROID)
void TabSpecificContentSettings::OnProtectedMediaIdentifierPermissionSet(
    const GURL& requesting_origin,
    bool allowed) {
  if (allowed) {
    OnContentAllowed(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
  } else {
    OnContentBlocked(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
  }
}
#endif

TabSpecificContentSettings::MicrophoneCameraState
TabSpecificContentSettings::GetMicrophoneCameraState() const {
  return microphone_camera_state_;
}

bool TabSpecificContentSettings::IsMicrophoneCameraStateChanged() const {
  if ((microphone_camera_state_ & MICROPHONE_ACCESSED) &&
      ((microphone_camera_state_& MICROPHONE_BLOCKED) ?
        !IsContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) :
        !IsContentAllowed(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)))
    return true;

  if ((microphone_camera_state_ & CAMERA_ACCESSED) &&
      ((microphone_camera_state_ & CAMERA_BLOCKED) ?
        !IsContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) :
        !IsContentAllowed(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)))
    return true;

  PrefService* prefs =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())->
          GetPrefs();
  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();

  if ((microphone_camera_state_ & MICROPHONE_ACCESSED) &&
      prefs->GetString(prefs::kDefaultAudioCaptureDevice) !=
      media_stream_selected_audio_device() &&
      media_indicator->IsCapturingAudio(web_contents()))
    return true;

  if ((microphone_camera_state_ & CAMERA_ACCESSED) &&
      prefs->GetString(prefs::kDefaultVideoCaptureDevice) !=
      media_stream_selected_video_device() &&
      media_indicator->IsCapturingVideo(web_contents()))
    return true;

  return false;
}

void TabSpecificContentSettings::OnMediaStreamPermissionSet(
    const GURL& request_origin,
    const MediaStreamDevicesController::MediaStreamTypeSettingsMap&
        request_permissions) {
  media_stream_access_origin_ = request_origin;
  MicrophoneCameraState prev_microphone_camera_state = microphone_camera_state_;
  microphone_camera_state_ = MICROPHONE_CAMERA_NOT_ACCESSED;

  PrefService* prefs =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())->
      GetPrefs();
  MediaStreamDevicesController::MediaStreamTypeSettingsMap::const_iterator it =
      request_permissions.find(content::MEDIA_DEVICE_AUDIO_CAPTURE);
  if (it != request_permissions.end()) {
    media_stream_requested_audio_device_ = it->second.requested_device_id;
    media_stream_selected_audio_device_ =
        media_stream_requested_audio_device_.empty() ?
            prefs->GetString(prefs::kDefaultAudioCaptureDevice) :
            media_stream_requested_audio_device_;
    DCHECK_NE(MediaStreamDevicesController::MEDIA_NONE, it->second.permission);
    bool mic_allowed =
        it->second.permission == MediaStreamDevicesController::MEDIA_ALLOWED;
    content_allowed_[CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC] = mic_allowed;
    content_blocked_[CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC] = !mic_allowed;
    microphone_camera_state_ |=
        MICROPHONE_ACCESSED | (mic_allowed ? 0 : MICROPHONE_BLOCKED);
  }

  it = request_permissions.find(content::MEDIA_DEVICE_VIDEO_CAPTURE);
  if (it != request_permissions.end()) {
    media_stream_requested_video_device_ = it->second.requested_device_id;
    media_stream_selected_video_device_ =
        media_stream_requested_video_device_.empty() ?
        prefs->GetString(prefs::kDefaultVideoCaptureDevice) :
        media_stream_requested_video_device_;
    DCHECK_NE(MediaStreamDevicesController::MEDIA_NONE, it->second.permission);
    bool cam_allowed =
        it->second.permission == MediaStreamDevicesController::MEDIA_ALLOWED;
    content_allowed_[CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA] = cam_allowed;
    content_blocked_[CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA] = !cam_allowed;
    microphone_camera_state_ |=
        CAMERA_ACCESSED | (cam_allowed ? 0 : CAMERA_BLOCKED);
  }

  if (microphone_camera_state_ != prev_microphone_camera_state) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
        content::Source<WebContents>(web_contents()),
        content::NotificationService::NoDetails());
  }
}

void TabSpecificContentSettings::OnMidiSysExAccessed(
    const GURL& requesting_origin) {
  midi_usages_state_.OnPermissionSet(requesting_origin, true);
  OnContentAllowed(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
}

void TabSpecificContentSettings::OnMidiSysExAccessBlocked(
    const GURL& requesting_origin) {
  midi_usages_state_.OnPermissionSet(requesting_origin, false);
  OnContentBlocked(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
}

void TabSpecificContentSettings::ClearBlockedContentSettingsExceptForCookies() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i) {
    if (i == CONTENT_SETTINGS_TYPE_COOKIES)
      continue;
    content_blocked_[i] = false;
    content_allowed_[i] = false;
    content_blockage_indicated_to_user_[i] = false;
  }
  microphone_camera_state_ = MICROPHONE_CAMERA_NOT_ACCESSED;
  load_plugins_link_enabled_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void TabSpecificContentSettings::ClearCookieSpecificContentSettings() {
  blocked_local_shared_objects_.Reset();
  allowed_local_shared_objects_.Reset();
  content_blocked_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_allowed_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void TabSpecificContentSettings::SetDownloadsBlocked(bool blocked) {
  content_blocked_[CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS] = blocked;
  content_allowed_[CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS] = !blocked;
  content_blockage_indicated_to_user_[
    CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS] = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void TabSpecificContentSettings::SetPopupsBlocked(bool blocked) {
  content_blocked_[CONTENT_SETTINGS_TYPE_POPUPS] = blocked;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_POPUPS] = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void TabSpecificContentSettings::GeolocationDidNavigate(
      const content::LoadCommittedDetails& details) {
  geolocation_usages_state_.DidNavigate(details);
}

void TabSpecificContentSettings::MidiDidNavigate(
    const content::LoadCommittedDetails& details) {
  midi_usages_state_.DidNavigate(details);
}

void TabSpecificContentSettings::ClearGeolocationContentSettings() {
  geolocation_usages_state_.ClearStateMap();
}

void TabSpecificContentSettings::ClearMidiContentSettings() {
  midi_usages_state_.ClearStateMap();
}

void TabSpecificContentSettings::SetPepperBrokerAllowed(bool allowed) {
  if (allowed) {
    OnContentAllowed(CONTENT_SETTINGS_TYPE_PPAPI_BROKER);
  } else {
    OnContentBlocked(CONTENT_SETTINGS_TYPE_PPAPI_BROKER);
  }
}

void TabSpecificContentSettings::RenderFrameForInterstitialPageCreated(
    content::RenderFrameHost* render_frame_host) {
  // We want to tell the renderer-side code to ignore content settings for this
  // page.
  render_frame_host->Send(new ChromeViewMsg_SetAsInterstitial(
      render_frame_host->GetRoutingID()));
}

bool TabSpecificContentSettings::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabSpecificContentSettings, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabSpecificContentSettings::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (!details.is_in_page) {
    // Clear "blocked" flags.
    ClearBlockedContentSettingsExceptForCookies();
    GeolocationDidNavigate(details);
    MidiDidNavigate(details);
  }
}

void TabSpecificContentSettings::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  if (render_frame_host->GetParent())
    return;

  // If we're displaying a network error page do not reset the content
  // settings delegate's cookies so the user has a chance to modify cookie
  // settings.
  if (!is_error_page)
    ClearCookieSpecificContentSettings();
  ClearGeolocationContentSettings();
  ClearMidiContentSettings();
  ClearPendingProtocolHandler();
}

void TabSpecificContentSettings::AppCacheAccessed(const GURL& manifest_url,
                                                  bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
    OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}

void TabSpecificContentSettings::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  const ContentSettingsDetails details(
      primary_pattern, secondary_pattern, content_type, resource_identifier);
  const NavigationController& controller = web_contents()->GetController();
  NavigationEntry* entry = controller.GetVisibleEntry();
  GURL entry_url;
  if (entry)
    entry_url = entry->GetURL();
  if (details.update_all() ||
      // The visible NavigationEntry is the URL in the URL field of a tab.
      // Currently this should be matched by the |primary_pattern|.
      details.primary_pattern().Matches(entry_url)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    const HostContentSettingsMap* map = profile->GetHostContentSettingsMap();

    if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
        content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
      const GURL media_origin = media_stream_access_origin();
      ContentSetting setting = map->GetContentSetting(media_origin,
                                                      media_origin,
                                                      content_type,
                                                      std::string());
      content_allowed_[content_type] = setting == CONTENT_SETTING_ALLOW;
      content_blocked_[content_type] = setting == CONTENT_SETTING_BLOCK;
    }
    RendererContentSettingRules rules;
    GetRendererContentSettingRules(map, &rules);
    Send(new ChromeViewMsg_SetContentSettingRules(rules));
  }
}

void TabSpecificContentSettings::AddSiteDataObserver(
    SiteDataObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TabSpecificContentSettings::RemoveSiteDataObserver(
    SiteDataObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TabSpecificContentSettings::NotifySiteDataObservers() {
  FOR_EACH_OBSERVER(SiteDataObserver, observer_list_, OnSiteDataAccessed());
}
