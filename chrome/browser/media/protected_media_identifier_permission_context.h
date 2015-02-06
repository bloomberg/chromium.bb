// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include "chrome/browser/content_settings/permission_context_base.h"
#include "components/content_settings/core/common/permission_request_id.h"

#if defined(OS_CHROMEOS)
#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#endif

class Profile;

namespace views {
class Widget;
}

namespace content {
class RenderViewHost;
class WebContents;
}

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public PermissionContextBase {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(Profile* profile);

  // In addition to the base class flow checks that it is only code from
  // valid iframes. It also adds special logic when called through an extension.
  void RequestPermission(content::WebContents* web_contents,
                         const PermissionRequestID& id,
                         const GURL& requesting_origin,
                         bool user_gesture,
                         const BrowserPermissionCallback& callback) override;
  ContentSetting GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void CancelPermissionRequest(content::WebContents* web_contents,
                               const PermissionRequestID& id) override;

 private:
  ~ProtectedMediaIdentifierPermissionContext() override;

  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;

  // Returns whether "Protected content" is enabled. It can be disabled by a
  // user in the master switch in content settings, or by the device policy.
  bool IsProtectedMediaIdentifierEnabled() const;

#if defined(OS_CHROMEOS)
  void OnPlatformVerificationResult(
      content::WebContents* web_contents,
      const PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const BrowserPermissionCallback& callback,
      chromeos::attestation::PlatformVerificationFlow::ConsentResponse
          response);

  // |this| is shared among multiple WebContents, so we could receive multiple
  // permission requests. This map tracks all pending requests. Note that we
  // only allow one request per WebContents.
  typedef std::map<content::WebContents*,
                   std::pair<views::Widget*, PermissionRequestID>>
      PendingRequestMap;
  PendingRequestMap pending_requests_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<ProtectedMediaIdentifierPermissionContext> weak_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
