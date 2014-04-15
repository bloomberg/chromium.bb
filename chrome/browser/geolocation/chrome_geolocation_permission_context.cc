// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"

#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::APIPermission;
using extensions::ExtensionRegistry;

class GeolocationPermissionRequest : public PermissionBubbleRequest {
 public:
  GeolocationPermissionRequest(
      ChromeGeolocationPermissionContext* context,
      const PermissionRequestID& id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::Callback<void(bool)> callback,
      const std::string& display_languages);
  virtual ~GeolocationPermissionRequest();

  // PermissionBubbleDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetMessageTextFragment() const OVERRIDE;
  virtual bool HasUserGesture() const OVERRIDE;
  virtual GURL GetRequestingHostname() const OVERRIDE;
  virtual void PermissionGranted() OVERRIDE;
  virtual void PermissionDenied() OVERRIDE;
  virtual void Cancelled() OVERRIDE;
  virtual void RequestFinished() OVERRIDE;

 private:
  ChromeGeolocationPermissionContext* context_;
  PermissionRequestID id_;
  GURL requesting_frame_;
  bool user_gesture_;
  base::Callback<void(bool)> callback_;
  std::string display_languages_;
};

GeolocationPermissionRequest::GeolocationPermissionRequest(
    ChromeGeolocationPermissionContext* context,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> callback,
    const std::string& display_languages)
    : context_(context),
      id_(id),
      requesting_frame_(requesting_frame),
      user_gesture_(user_gesture),
      callback_(callback),
      display_languages_(display_languages) {}

GeolocationPermissionRequest::~GeolocationPermissionRequest() {}

int GeolocationPermissionRequest::GetIconID() const {
  return IDR_INFOBAR_GEOLOCATION;
}

base::string16 GeolocationPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_, display_languages_));
}

base::string16 GeolocationPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT);
}

bool GeolocationPermissionRequest::HasUserGesture() const {
  return user_gesture_;
}

GURL GeolocationPermissionRequest::GetRequestingHostname() const {
  return requesting_frame_;
}

void GeolocationPermissionRequest::PermissionGranted() {
  context_->NotifyPermissionSet(id_, requesting_frame_, callback_, true);
}

void GeolocationPermissionRequest::PermissionDenied() {
  context_->NotifyPermissionSet(id_, requesting_frame_, callback_, false);
}

void GeolocationPermissionRequest::Cancelled() {
  context_->NotifyPermissionSet(id_, requesting_frame_, callback_, false);
}

void GeolocationPermissionRequest::RequestFinished() {
  delete this;
}


ChromeGeolocationPermissionContext::ChromeGeolocationPermissionContext(
    Profile* profile)
    : profile_(profile),
      shutting_down_(false) {
}

ChromeGeolocationPermissionContext::~ChromeGeolocationPermissionContext() {
  // ChromeGeolocationPermissionContext may be destroyed on either the UI thread
  // or the IO thread, but the PermissionQueueController must have been
  // destroyed on the UI thread.
  DCHECK(!permission_queue_controller_.get());
}

void ChromeGeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> callback) {
  GURL requesting_frame_origin = requesting_frame.GetOrigin();
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromeGeolocationPermissionContext::RequestGeolocationPermission,
            this, render_process_id, render_view_id, bridge_id,
            requesting_frame_origin, user_gesture, callback));
    return;
  }

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);

  WebViewGuest* guest = WebViewGuest::FromWebContents(web_contents);
  if (guest) {
    guest->RequestGeolocationPermission(bridge_id,
                                        requesting_frame,
                                        user_gesture,
                                        callback);
    return;
  }
  const PermissionRequestID id(render_process_id, render_view_id, bridge_id, 0);
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  if (extension_registry) {
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetExtensionOrAppByURL(
            requesting_frame_origin);
    if (IsExtensionWithPermissionOrSuggestInConsole(APIPermission::kGeolocation,
                                                    extension,
                                                    profile_)) {
      // Make sure the extension is in the calling process.
      if (extensions::ProcessMap::Get(profile_)
              ->Contains(extension->id(), id.render_process_id())) {
        NotifyPermissionSet(id, requesting_frame_origin, callback, true);
        return;
      }
    }
  }

  if (extensions::GetViewType(web_contents) !=
      extensions::VIEW_TYPE_TAB_CONTENTS) {
    // The tab may have gone away, or the request may not be from a tab at all.
    // TODO(mpcomplete): the request could be from a background page or
    // extension popup (web_contents will have a different ViewType). But why do
    // we care? Shouldn't we still put an infobar up in the current tab?
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
                 << id.ToString()
                 << " (can't prompt user without a visible tab)";
    NotifyPermissionSet(id, requesting_frame_origin, callback, false);
    return;
  }

  GURL embedder = web_contents->GetLastCommittedURL().GetOrigin();
  if (!requesting_frame_origin.is_valid() || !embedder.is_valid()) {
    LOG(WARNING) << "Attempt to use geolocation from an invalid URL: "
                 << requesting_frame_origin << "," << embedder
                 << " (geolocation is not supported in popups)";
    NotifyPermissionSet(id, requesting_frame_origin, callback, false);
    return;
  }

  DecidePermission(web_contents, id, requesting_frame_origin, user_gesture,
                   embedder, "", callback);
}

void ChromeGeolocationPermissionContext::CancelGeolocationPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromeGeolocationPermissionContext::
                CancelGeolocationPermissionRequest,
            this,
            render_process_id,
            render_view_id,
            bridge_id,
            requesting_frame));
     return;
  }

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  WebViewGuest* guest =
      web_contents ? WebViewGuest::FromWebContents(web_contents) : NULL;
  if (guest) {
    guest->CancelGeolocationPermissionRequest(bridge_id);
    return;
  }
  // TODO(gbillock): cancel permission bubble request.
  CancelPendingInfobarRequest(PermissionRequestID(
      render_process_id, render_view_id, bridge_id, 0));
}

void ChromeGeolocationPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    const GURL& embedder,
    const std::string& accept_button_label,
    base::Callback<void(bool)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ContentSetting content_setting =
     profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame, embedder, CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string());
  switch (content_setting) {
    case CONTENT_SETTING_BLOCK:
      PermissionDecided(id, requesting_frame, embedder, callback, false);
      break;
    case CONTENT_SETTING_ALLOW:
      PermissionDecided(id, requesting_frame, embedder, callback, true);
      break;
    default:
      if (PermissionBubbleManager::Enabled()) {
        PermissionBubbleManager* mgr =
            PermissionBubbleManager::FromWebContents(web_contents);
        mgr->AddRequest(new GeolocationPermissionRequest(
                this, id, requesting_frame, user_gesture, callback,
                profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)));
      } else {
        // setting == ask. Prompt the user.
        QueueController()->CreateInfoBarRequest(
            id, requesting_frame, embedder, accept_button_label,
                base::Bind(
                    &ChromeGeolocationPermissionContext::NotifyPermissionSet,
                base::Unretained(this), id, requesting_frame, callback));
      }
  }
}

void ChromeGeolocationPermissionContext::CreateInfoBarRequest(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    const std::string accept_button_label,
    base::Callback<void(bool)> callback) {
    QueueController()->CreateInfoBarRequest(
        id, requesting_frame, embedder, accept_button_label, base::Bind(
            &ChromeGeolocationPermissionContext::NotifyPermissionSet,
            base::Unretained(this), id, requesting_frame, callback));
}

void ChromeGeolocationPermissionContext::ShutdownOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  permission_queue_controller_.reset();
  shutting_down_ = true;
}

void ChromeGeolocationPermissionContext::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback,
    bool allowed) {
  NotifyPermissionSet(id, requesting_frame, callback, allowed);
}

void ChromeGeolocationPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    base::Callback<void(bool)> callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // WebContents may have gone away (or not exists for extension).
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings) {
    content_settings->OnGeolocationPermissionSet(requesting_frame.GetOrigin(),
                                                 allowed);
  }

  callback.Run(allowed);
}

PermissionQueueController*
    ChromeGeolocationPermissionContext::QueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!shutting_down_);
  if (!permission_queue_controller_)
    permission_queue_controller_.reset(CreateQueueController());
  return permission_queue_controller_.get();
}

PermissionQueueController*
    ChromeGeolocationPermissionContext::CreateQueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return new PermissionQueueController(profile(),
                                       CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

void ChromeGeolocationPermissionContext::CancelPendingInfobarRequest(
    const PermissionRequestID& id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;

  // TODO(gbillock): handle permission bubble cancellation.
  QueueController()->CancelInfoBarRequest(id);
}
