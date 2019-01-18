// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/per_device_provisioning_permission.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/android/media_drm_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {

// A PermissionRequest to allow MediaDrmBridge to use per-device provisioning.
class PerDeviceProvisioningPermissionRequest : public PermissionRequest {
 public:
  PerDeviceProvisioningPermissionRequest(
      const url::Origin& origin,
      base::OnceCallback<void(bool)> callback)
      : origin_(origin), callback_(std::move(callback)) {}

  PermissionRequest::IconId GetIconId() const final {
    return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
  }

  base::string16 GetMessageText() const final {
    // Note that the string is specific to per-device provisioning.
    return l10n_util::GetStringFUTF16(
        IDS_PROTECTED_MEDIA_IDENTIFIER_PER_DEVICE_PROVISIONING_INFOBAR_TEXT,
        url_formatter::FormatUrlForSecurityDisplay(
            GetOrigin(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  }

  base::string16 GetMessageTextFragment() const final {
    return l10n_util::GetStringUTF16(
        IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT);
  }

  GURL GetOrigin() const final { return origin_.GetURL(); }

  void PermissionGranted() final { std::move(callback_).Run(true); }

  void PermissionDenied() final { std::move(callback_).Run(false); }

  void Cancelled() final { std::move(callback_).Run(false); }

  void RequestFinished() final {
    // The |callback_| may not have run if the prompt was ignored, e.g. the tab
    // was closed while the prompt was displayed.
    if (callback_)
      std::move(callback_).Run(false);

    delete this;
  }

  PermissionRequestType GetPermissionRequestType() const final {
    return PermissionRequestType::PERMISSION_PROTECTED_MEDIA_IDENTIFIER;
  }

 private:
  // Can only be self-destructed. See RequestFinished().
  ~PerDeviceProvisioningPermissionRequest() final = default;

  const url::Origin origin_;
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(PerDeviceProvisioningPermissionRequest);
};

}  // namespace

void RequestPerDeviceProvisioningPermission(
    content::RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);
  DCHECK(callback);
  DCHECK(media::MediaDrmBridge::IsPerOriginProvisioningSupported())
      << "RequestPerDeviceProvisioningPermission() should only be called when "
         "per-origin provisioning is supported.";

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents) << "WebContents not available.";

  auto* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    std::move(callback).Run(false);
    return;
  }

  // The created PerDeviceProvisioningPermissionRequest deletes itself once
  // complete. See PerDeviceProvisioningPermissionRequest::RequestFinished().
  permission_request_manager->AddRequest(
      new PerDeviceProvisioningPermissionRequest(
          render_frame_host->GetLastCommittedOrigin(), std::move(callback)));
}
