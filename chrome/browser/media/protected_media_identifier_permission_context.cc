// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#include "ui/views/widget/widget.h"

using chromeos::attestation::PlatformVerificationDialog;
using chromeos::attestation::PlatformVerificationFlow;
#endif

#if defined(OS_CHROMEOS)
namespace {
PermissionRequestID GetInvalidPendingId() {
  return PermissionRequestID(-1, -1, -1, GURL());
}
}
#endif

ProtectedMediaIdentifierPermissionContext::
    ProtectedMediaIdentifierPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER)
#if defined(OS_CHROMEOS)
      ,
      pending_id_(GetInvalidPendingId()),
      widget_(nullptr),
      weak_factory_(this)
#endif
{
}

ProtectedMediaIdentifierPermissionContext::
    ~ProtectedMediaIdentifierPermissionContext() {
}

void ProtectedMediaIdentifierPermissionContext::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid() ||
      !IsProtectedMediaIdentifierEnabled()) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, false /* granted */);
    return;
  }

#if defined(OS_CHROMEOS)
  // On ChromeOS, we don't use PermissionContextBase::RequestPermission() which
  // uses the standard permission infobar/bubble UI. See http://crbug.com/454847
  // Instead, we check the content setting and show the existing platform
  // verification UI.
  // TODO(xhwang): Remove when http://crbug.com/454847 is fixed.
  ContentSetting content_setting =
      GetPermissionStatus(requesting_origin, embedding_origin);

  switch (content_setting) {
    case CONTENT_SETTING_BLOCK:
      NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                          false /* persist */, false /* granted */);
      return;
    case CONTENT_SETTING_ALLOW:
      NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                          false /* persist */, true /* granted */);
      return;
    default:
      break;
  }

  // We only support one prompt and one pending permission request.
  // Reject the new one if there is already one pending. See
  // http://crbug.com/447005
  if (!pending_id_.Equals(GetInvalidPendingId())) {
    callback.Run(false);
    return;
  }

  pending_id_ = id;
  widget_ = PlatformVerificationDialog::ShowDialog(
      web_contents, requesting_origin,
      base::Bind(&ProtectedMediaIdentifierPermissionContext::
                     OnPlatformVerificationResult,
                 weak_factory_.GetWeakPtr(), id, requesting_origin,
                 embedding_origin, callback));
#else
  PermissionContextBase::RequestPermission(web_contents, id, requesting_origin,
                                           user_gesture, callback);
#endif
}

ContentSetting ProtectedMediaIdentifierPermissionContext::GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const {
  if (!IsProtectedMediaIdentifierEnabled())
    return CONTENT_SETTING_BLOCK;

  return PermissionContextBase::GetPermissionStatus(requesting_origin,
                                                    embedding_origin);
}

void ProtectedMediaIdentifierPermissionContext::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_CHROMEOS)
  if (!widget_ || !pending_id_.Equals(id))
    return;

  // Close the |widget_|. OnPlatformVerificationResult() will be fired
  // during this process, but since |pending_id_| is cleared, the callback will
  // be dropped.
  pending_id_ = GetInvalidPendingId();
  widget_->Close();
  return;
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
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings) {
    content_settings->OnProtectedMediaIdentifierPermissionSet(
        requesting_frame.GetOrigin(), allowed);
  }
}

// TODO(xhwang): We should consolidate the "protected content" related pref
// across platforms.
bool ProtectedMediaIdentifierPermissionContext::
    IsProtectedMediaIdentifierEnabled() const {
  bool enabled = false;

#if defined(OS_ANDROID)
  enabled = profile()->GetPrefs()->GetBoolean(
      prefs::kProtectedMediaIdentifierEnabled);
#endif

#if defined(OS_CHROMEOS)
  // This could be disabled by the device policy.
  bool enabled_for_device = false;
  enabled = chromeos::CrosSettings::Get()->GetBoolean(
                chromeos::kAttestationForContentProtectionEnabled,
                &enabled_for_device) &&
            enabled_for_device &&
            profile()->GetPrefs()->GetBoolean(prefs::kEnableDRM);
#endif

  DVLOG_IF(1, !enabled)
      << "Protected media identifier disabled by the user or by device policy.";
  return enabled;
}

#if defined(OS_CHROMEOS)
void ProtectedMediaIdentifierPermissionContext::OnPlatformVerificationResult(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    chromeos::attestation::PlatformVerificationFlow::ConsentResponse response) {
  DCHECK(widget_);
  widget_ = nullptr;

  // The request may have been canceled. Drop the callback here.
  if (!pending_id_.Equals(id))
    return;

  pending_id_ = GetInvalidPendingId();

  if (response == PlatformVerificationFlow::CONSENT_RESPONSE_NONE) {
    // Deny request and do not save to content settings.
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false,   // Do not save to content settings.
                        false);  // Do not allow the permission.
    return;
  }

  NotifyPermissionSet(
      id, requesting_origin, embedding_origin, callback,
      true,  // Save to content settings.
      response == PlatformVerificationFlow::CONSENT_RESPONSE_ALLOW);
}
#endif
