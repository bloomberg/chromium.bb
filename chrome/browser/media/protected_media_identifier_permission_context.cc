// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context.h"

#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"

using extensions::APIPermission;
#endif

ProtectedMediaIdentifierPermissionContext::
    ProtectedMediaIdentifierPermissionContext(Profile* profile)
    : profile_(profile), shutting_down_(false) {}

ProtectedMediaIdentifierPermissionContext::
    ~ProtectedMediaIdentifierPermissionContext() {
  // ProtectedMediaIdentifierPermissionContext may be destroyed on either
  // the UI thread or the IO thread, but the PermissionQueueController must have
  // been destroyed on the UI thread.
  DCHECK(!permission_queue_controller_.get());
}

void ProtectedMediaIdentifierPermissionContext::
    RequestProtectedMediaIdentifierPermission(
        content::WebContents* web_contents,
        const GURL& origin,
        base::Callback<void(bool)> result_callback,
        base::Closure* cancel_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;

  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  if (cancel_callback) {
    *cancel_callback = base::Bind(
        &ProtectedMediaIdentifierPermissionContext::
            CancelProtectedMediaIdentifierPermissionRequests,
        this, render_process_id, render_view_id, origin);
  }

  const PermissionRequestID id(
      render_process_id, render_view_id, 0, origin);

#if defined(ENABLE_EXTENSIONS)
  if (extensions::GetViewType(web_contents) !=
      extensions::VIEW_TYPE_TAB_CONTENTS) {
    // The tab may have gone away, or the request may not be from a tab at all.
    LOG(WARNING)
        << "Attempt to use protected media identifier in tabless renderer: "
        << id.ToString()
        << " (can't prompt user without a visible tab)";
    NotifyPermissionSet(id, origin, result_callback, false);
    return;
  }
#endif

  GURL embedder = web_contents->GetLastCommittedURL();
  if (!origin.is_valid() || !embedder.is_valid()) {
    LOG(WARNING)
        << "Attempt to use protected media identifier from an invalid URL: "
        << origin << "," << embedder
        << " (proteced media identifier is not supported in popups)";
    NotifyPermissionSet(id, origin, result_callback, false);
    return;
  }

  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  DecidePermission(id, origin, embedder, rvh, result_callback);
}

void ProtectedMediaIdentifierPermissionContext::
    CancelProtectedMediaIdentifierPermissionRequests(
        int render_process_id,
        int render_view_id,
        const GURL& origin) {
  CancelPendingInfobarRequests(
      render_process_id, render_view_id, origin);
}

void ProtectedMediaIdentifierPermissionContext::DecidePermission(
    const PermissionRequestID& id,
    const GURL& origin,
    const GURL& embedder,
    content::RenderViewHost* rvh,
    const base::Callback<void(bool)>& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if defined(OS_ANDROID)
  // Check if the protected media identifier master switch is disabled.
  if (!profile()->GetPrefs()->GetBoolean(
        prefs::kProtectedMediaIdentifierEnabled)) {
    PermissionDecided(id, origin, embedder, callback, false);
    return;
  }
#endif

  ContentSetting content_setting =
     profile_->GetHostContentSettingsMap()->GetContentSetting(
          origin,
          embedder,
          CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
          std::string());
  switch (content_setting) {
    case CONTENT_SETTING_BLOCK:
      PermissionDecided(id, origin, embedder, callback, false);
      break;
    case CONTENT_SETTING_ALLOW:
      PermissionDecided(id, origin, embedder, callback, true);
      break;
    case CONTENT_SETTING_ASK:
      QueueController()->CreateInfoBarRequest(
          id,
          origin,
          embedder,
          base::Bind(&ProtectedMediaIdentifierPermissionContext::
                          NotifyPermissionSet,
                     base::Unretained(this),
                     id,
                     origin,
                     callback));
      break;
    default:
      NOTREACHED();
  }
}

void ProtectedMediaIdentifierPermissionContext::ShutdownOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  permission_queue_controller_.reset();
  shutting_down_ = true;
}

void ProtectedMediaIdentifierPermissionContext::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& origin,
    const GURL& embedder,
    const base::Callback<void(bool)>& callback,
    bool allowed) {
  NotifyPermissionSet(id, origin, callback, allowed);
}

void ProtectedMediaIdentifierPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& origin,
    const base::Callback<void(bool)>& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // WebContents may have gone away.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings) {
    content_settings->OnProtectedMediaIdentifierPermissionSet(
        origin.GetOrigin(), allowed);
  }

  callback.Run(allowed);
}

PermissionQueueController*
    ProtectedMediaIdentifierPermissionContext::QueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!shutting_down_);
  if (!permission_queue_controller_)
    permission_queue_controller_.reset(CreateQueueController());
  return permission_queue_controller_.get();
}

PermissionQueueController*
    ProtectedMediaIdentifierPermissionContext::CreateQueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return new PermissionQueueController(
      profile(), CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
}

void
ProtectedMediaIdentifierPermissionContext::CancelPendingInfobarRequests(
    int render_process_id,
    int render_view_id,
    const GURL& origin) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ProtectedMediaIdentifierPermissionContext::
                        CancelPendingInfobarRequests,
                   this,
                   render_process_id,
                   render_view_id,
                   origin));
    return;
  }
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;
  QueueController()->CancelInfoBarRequest(
      PermissionRequestID(render_process_id, render_view_id, 0,
                          origin));
}
