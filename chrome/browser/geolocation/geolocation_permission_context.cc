// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

class GeolocationPermissionRequest : public PermissionBubbleRequest {
 public:
  GeolocationPermissionRequest(GeolocationPermissionContext* context,
                               const PermissionRequestID& id,
                               const GURL& requesting_frame,
                               const GURL& embedder,
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
  GeolocationPermissionContext* context_;
  PermissionRequestID id_;
  GURL requesting_frame_;
  GURL embedder_;
  bool user_gesture_;
  base::Callback<void(bool)> callback_;
  std::string display_languages_;
};

GeolocationPermissionRequest::GeolocationPermissionRequest(
    GeolocationPermissionContext* context,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool user_gesture,
    base::Callback<void(bool)> callback,
    const std::string& display_languages)
    : context_(context),
      id_(id),
      requesting_frame_(requesting_frame),
      embedder_(embedder),
      user_gesture_(user_gesture),
      callback_(callback),
      display_languages_(display_languages) {}

GeolocationPermissionRequest::~GeolocationPermissionRequest() {}

int GeolocationPermissionRequest::GetIconID() const {
  return IDR_INFOBAR_GEOLOCATION;
}

base::string16 GeolocationPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_GEOLOCATION_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_.GetOrigin(), display_languages_,
                     net::kFormatUrlOmitUsernamePassword |
                     net::kFormatUrlOmitTrailingSlashOnBareHostname,
                     net::UnescapeRule::SPACES, NULL, NULL, NULL));
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
  context_->QueueController()->UpdateContentSetting(
      requesting_frame_, embedder_, true);
  context_->NotifyPermissionSet(id_, requesting_frame_, callback_, true);
}

void GeolocationPermissionRequest::PermissionDenied() {
  context_->QueueController()->UpdateContentSetting(
      requesting_frame_, embedder_, false);
  context_->NotifyPermissionSet(id_, requesting_frame_, callback_, false);
}

void GeolocationPermissionRequest::Cancelled() {
}

void GeolocationPermissionRequest::RequestFinished() {
  // Deletes 'this'.
  context_->RequestFinished(this);
}


GeolocationPermissionContext::GeolocationPermissionContext(
    Profile* profile)
    : profile_(profile),
      shutting_down_(false),
      extensions_context_(profile) {
}

GeolocationPermissionContext::~GeolocationPermissionContext() {
  // GeolocationPermissionContext may be destroyed on either the UI thread
  // or the IO thread, but the PermissionQueueController must have been
  // destroyed on the UI thread.
  DCHECK(!permission_queue_controller_.get());
}

void GeolocationPermissionContext::RequestGeolocationPermission(
    content::WebContents* web_contents,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> result_callback,
    base::Closure* cancel_callback) {
  GURL requesting_frame_origin = requesting_frame.GetOrigin();
  if (shutting_down_)
    return;

  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  if (cancel_callback) {
    *cancel_callback = base::Bind(
        &GeolocationPermissionContext::CancelGeolocationPermissionRequest,
        this, render_process_id, render_view_id, bridge_id);
  }

  const PermissionRequestID id(
      render_process_id, render_view_id, bridge_id, GURL());

  bool permission_set;
  bool new_permission;
  if (extensions_context_.RequestPermission(
          web_contents, id, bridge_id, requesting_frame, user_gesture,
          result_callback, &permission_set, &new_permission)) {
    if (permission_set) {
      NotifyPermissionSet(id, requesting_frame_origin, result_callback,
                          new_permission);
    }
    return;
  }

  GURL embedder = web_contents->GetLastCommittedURL().GetOrigin();
  if (!requesting_frame_origin.is_valid() || !embedder.is_valid()) {
    LOG(WARNING) << "Attempt to use geolocation from an invalid URL: "
                 << requesting_frame_origin << "," << embedder
                 << " (geolocation is not supported in popups)";
    NotifyPermissionSet(id, requesting_frame_origin, result_callback, false);
    return;
  }

  DecidePermission(web_contents, id, requesting_frame_origin, user_gesture,
                   embedder, result_callback);
}

void GeolocationPermissionContext::CancelGeolocationPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id) {
  content::WebContents* web_contents = tab_util::GetWebContentsByID(
        render_process_id, render_view_id);
  if (extensions_context_.CancelPermissionRequest(web_contents, bridge_id))
    return;

  CancelPendingInfobarRequest(PermissionRequestID(
      render_process_id, render_view_id, bridge_id, GURL()));
}

void GeolocationPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    const GURL& embedder,
    base::Callback<void(bool)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ContentSetting content_setting =
      profile_->GetHostContentSettingsMap()
          ->GetContentSettingAndMaybeUpdateLastUsage(
              requesting_frame,
              embedder,
              CONTENT_SETTINGS_TYPE_GEOLOCATION,
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
        if (mgr) {
          scoped_ptr<GeolocationPermissionRequest> request_ptr(
              new GeolocationPermissionRequest(
                  this, id, requesting_frame, embedder, user_gesture, callback,
                  profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)));
          GeolocationPermissionRequest* request = request_ptr.get();
          pending_requests_.add(id.ToString(), request_ptr.Pass());
          mgr->AddRequest(request);
        }
      } else {
        // setting == ask. Prompt the user.
        QueueController()->CreateInfoBarRequest(
            id, requesting_frame, embedder,
                base::Bind(
                    &GeolocationPermissionContext::NotifyPermissionSet,
                base::Unretained(this), id, requesting_frame, callback));
      }
  }
}

void GeolocationPermissionContext::CreateInfoBarRequest(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback) {
    QueueController()->CreateInfoBarRequest(
        id, requesting_frame, embedder, base::Bind(
            &GeolocationPermissionContext::NotifyPermissionSet,
            base::Unretained(this), id, requesting_frame, callback));
}

void GeolocationPermissionContext::RequestFinished(
    GeolocationPermissionRequest* request) {
  base::ScopedPtrHashMap<std::string,
                         GeolocationPermissionRequest>::iterator it;
  for (it = pending_requests_.begin(); it != pending_requests_.end(); ++it) {
    if (it->second == request) {
      pending_requests_.take_and_erase(it);
      return;
    }
  }
}


void GeolocationPermissionContext::ShutdownOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  permission_queue_controller_.reset();
  shutting_down_ = true;
}

void GeolocationPermissionContext::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback,
    bool allowed) {
  NotifyPermissionSet(id, requesting_frame, callback, allowed);
}

void GeolocationPermissionContext::NotifyPermissionSet(
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
    GeolocationPermissionContext::QueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!shutting_down_);
  if (!permission_queue_controller_)
    permission_queue_controller_.reset(CreateQueueController());
  return permission_queue_controller_.get();
}

PermissionQueueController*
    GeolocationPermissionContext::CreateQueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return new PermissionQueueController(profile(),
                                       CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

void GeolocationPermissionContext::CancelPendingInfobarRequest(
    const PermissionRequestID& id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;

  if (PermissionBubbleManager::Enabled()) {
    GeolocationPermissionRequest* cancelling =
        pending_requests_.get(id.ToString());
    content::WebContents* web_contents = tab_util::GetWebContentsByID(
        id.render_process_id(), id.render_view_id());
    if (cancelling != NULL && web_contents != NULL &&
        PermissionBubbleManager::FromWebContents(web_contents) != NULL) {
      PermissionBubbleManager::FromWebContents(web_contents)->
          CancelRequest(cancelling);
    }
    return;
  }

  QueueController()->CancelInfoBarRequest(id);
}
