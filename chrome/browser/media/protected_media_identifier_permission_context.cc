// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#if defined(OS_CHROMEOS)
#include <utility>

#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/views/widget/widget.h"
#elif !defined(OS_ANDROID)
#error This file currently only supports Chrome OS and Android.
#endif

#if defined(OS_CHROMEOS)
using chromeos::attestation::PlatformVerificationDialog;
#endif

ProtectedMediaIdentifierPermissionContext::
    ProtectedMediaIdentifierPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            content::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                            CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER)
#if defined(OS_CHROMEOS)
      ,
      weak_factory_(this)
#endif
{
}

ProtectedMediaIdentifierPermissionContext::
    ~ProtectedMediaIdentifierPermissionContext() {
}

#if defined(OS_CHROMEOS)
void ProtectedMediaIdentifierPermissionContext::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // First check if this permission has been disabled. This check occurs before
  // the call to GetPermissionStatus, which will return CONTENT_SETTING_BLOCK
  // if the kill switch is on.
  //
  // TODO(xhwang): Remove this kill switch block when crbug.com/454847 is fixed
  // and we no longer call GetPermissionStatus before
  // PermissionContextBase::RequestPermission.
  if (IsPermissionKillSwitchOn()) {
    // Log to the developer console.
    web_contents->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_LOG,
        base::StringPrintf(
            "%s permission has been blocked.",
            PermissionUtil::GetPermissionString(
                content::PermissionType::PROTECTED_MEDIA_IDENTIFIER)
                .c_str()));
    // The kill switch is enabled for this permission; Block all requests and
    // run the callback immediately.
    callback.Run(CONTENT_SETTING_BLOCK);
    return;
  }

  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();

  DVLOG(1) << __FUNCTION__ << ": (" << requesting_origin.spec() << ", "
           << embedding_origin.spec() << ")";

  ContentSetting content_setting =
      GetPermissionStatus(requesting_origin, embedding_origin);

  if (content_setting == CONTENT_SETTING_ALLOW ||
      content_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, content_setting);
    return;
  }

  DCHECK_EQ(CONTENT_SETTING_ASK, content_setting);

  // Since the dialog is modal, we only support one prompt per |web_contents|.
  // Reject the new one if there is already one pending. See
  // http://crbug.com/447005
  if (pending_requests_.count(web_contents)) {
    callback.Run(CONTENT_SETTING_ASK);
    return;
  }

  // On ChromeOS, we don't use PermissionContextBase::RequestPermission() which
  // uses the standard permission infobar/bubble UI. See http://crbug.com/454847
  // Instead, we show the existing platform verification UI.
  // TODO(xhwang): Remove when http://crbug.com/454847 is fixed.
  views::Widget* widget = PlatformVerificationDialog::ShowDialog(
      web_contents, requesting_origin,
      base::Bind(&ProtectedMediaIdentifierPermissionContext::
                     OnPlatformVerificationConsentResponse,
                 weak_factory_.GetWeakPtr(), web_contents, id,
                 requesting_origin, embedding_origin, callback));
  pending_requests_.insert(
      std::make_pair(web_contents, std::make_pair(widget, id)));
}
#endif  // defined(OS_CHROMEOS)

ContentSetting ProtectedMediaIdentifierPermissionContext::GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const {
  DVLOG(1) << __FUNCTION__ << ": (" << requesting_origin.spec() << ", "
           << embedding_origin.spec() << ")";

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid() ||
      !IsProtectedMediaIdentifierEnabled()) {
    return CONTENT_SETTING_BLOCK;
  }

  ContentSetting content_setting = PermissionContextBase::GetPermissionStatus(
      requesting_origin, embedding_origin);
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_ASK);

  return content_setting;
}

void ProtectedMediaIdentifierPermissionContext::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_CHROMEOS)
  PendingRequestMap::iterator request = pending_requests_.find(web_contents);
  if (request == pending_requests_.end() || (request->second.second != id))
    return;

  views::Widget* widget = request->second.first;
  pending_requests_.erase(request);

  // If |web_contents| is being destroyed, |widget| could be invalid. No need to
  // manually close it here. Otherwise, close the |widget| here.
  // OnPlatformVerificationConsentResponse() will be fired during this process,
  // but since |web_contents| is removed from |pending_requests_|, the callback
  // will simply be dropped.
  if (!web_contents->IsBeingDestroyed())
    widget->Close();
#else
  PermissionContextBase::CancelPermissionRequest(web_contents, id);
#endif
}

void ProtectedMediaIdentifierPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // WebContents may have gone away.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::GetForFrame(id.render_process_id(),
                                              id.render_frame_id());
  if (content_settings) {
    content_settings->OnProtectedMediaIdentifierPermissionSet(
        requesting_frame.GetOrigin(), allowed);
  }
}

bool
ProtectedMediaIdentifierPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}

// TODO(xhwang): We should consolidate the "protected content" related pref
// across platforms.
bool ProtectedMediaIdentifierPermissionContext::
    IsProtectedMediaIdentifierEnabled() const {
#if defined(OS_CHROMEOS)
  // Platform verification is not allowed in incognito or guest mode.
  if (profile()->IsOffTheRecord() || profile()->IsGuestSession()) {
    DVLOG(1) << "Protected media identifier disabled in incognito or guest "
                "mode.";
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kSystemDevMode) &&
      !command_line->HasSwitch(chromeos::switches::kAllowRAInDevMode)) {
    DVLOG(1) << "Protected media identifier disabled in dev mode.";
    return false;
  }

  // This could be disabled by the device policy or by user's master switch.
  bool enabled_for_device = false;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kAttestationForContentProtectionEnabled,
          &enabled_for_device) ||
      !enabled_for_device ||
      !profile()->GetPrefs()->GetBoolean(prefs::kEnableDRM)) {
    DVLOG(1) << "Protected media identifier disabled by the user or by device "
                "policy.";
    return false;
  }
#endif

  return true;
}

#if defined(OS_CHROMEOS)
void ProtectedMediaIdentifierPermissionContext::
    OnPlatformVerificationConsentResponse(
        content::WebContents* web_contents,
        const PermissionRequestID& id,
        const GURL& requesting_origin,
        const GURL& embedding_origin,
        const BrowserPermissionCallback& callback,
        PlatformVerificationDialog::ConsentResponse response) {
  // The request may have been canceled. Drop the callback in that case.
  PendingRequestMap::iterator request = pending_requests_.find(web_contents);
  if (request == pending_requests_.end())
    return;

  DCHECK(request->second.second == id);
  pending_requests_.erase(request);

  ContentSetting content_setting = CONTENT_SETTING_ASK;
  bool persist = false; // Whether the ContentSetting should be saved.
  switch (response) {
    case PlatformVerificationDialog::CONSENT_RESPONSE_NONE:
      content_setting = CONTENT_SETTING_ASK;
      persist = false;
      break;
    case PlatformVerificationDialog::CONSENT_RESPONSE_ALLOW:
      VLOG(1) << "Platform verification accepted by user.";
      content::RecordAction(
          base::UserMetricsAction("PlatformVerificationAccepted"));
      content_setting = CONTENT_SETTING_ALLOW;
      persist = true;
      break;
    case PlatformVerificationDialog::CONSENT_RESPONSE_DENY:
      VLOG(1) << "Platform verification denied by user.";
      content::RecordAction(
          base::UserMetricsAction("PlatformVerificationRejected"));
      content_setting = CONTENT_SETTING_BLOCK;
      persist = true;
      break;
  }

  NotifyPermissionSet(
      id, requesting_origin, embedding_origin, callback,
      persist, content_setting);
}
#endif
