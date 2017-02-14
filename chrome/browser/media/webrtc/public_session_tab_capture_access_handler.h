// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_PUBLIC_SESSION_TAB_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_PUBLIC_SESSION_TAB_CAPTURE_ACCESS_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/media/capture_access_handler_base.h"
#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/extension_id.h"

// MediaAccessHandler for TabCapture API in Public Sessions. This class is
// implemented as a wrapper around TabCaptureAccessHandler. It allows for finer
// access control to the TabCapture manifest permission feature inside of Public
// Sessions.
//
// In Public Sessions, extensions (and apps) are force-installed by admin policy
// so the user does not get a chance to review the permissions for these
// extensions. This is not acceptable from a security/privacy standpoint, so
// when an extension uses the TabCapture API for the first time, we show the
// user a dialog where they can choose whether to allow the extension access to
// the API.
//
// TODO(isandrk): Refactor common code out of this class and
// PublicSessionMediaAccessHandler (crbug.com/672620).
class PublicSessionTabCaptureAccessHandler : public CaptureAccessHandlerBase {
 public:
  PublicSessionTabCaptureAccessHandler();
  ~PublicSessionTabCaptureAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(const content::MediaStreamType type,
                          const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::WebContents* web_contents,
      const GURL& security_origin,
      content::MediaStreamType type,
      const extensions::Extension* extension) override;
  void HandleRequest(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     const content::MediaResponseCallback& callback,
                     const extensions::Extension* extension) override;

 private:
  // Helper function used to chain the HandleRequest call into the original
  // inside of TabCaptureAccessHandler.
  void ChainHandleRequest(content::WebContents* web_contents,
                          const content::MediaStreamRequest& request,
                          const content::MediaResponseCallback& callback,
                          const extensions::Extension* extension);

  // Function used to resolve user decision regarding allowing tab capture.
  void ResolvePermissionPrompt(content::WebContents* web_contents,
                               const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback,
                               const extensions::Extension* extension,
                               ExtensionInstallPrompt::Result prompt_result);

  // Class used to cache user choice regarding allowing tab capture.
  class UserChoice {
   public:
    // Helper function for checking if tab capture is allowed by user choice.
    bool IsAllowed() const;
    // Helper function which returns true if user choice wasn't prompted yet.
    bool NeedsPrompting() const;
    void Set(bool allowed);
    void SetPrompted();

   private:
    bool tab_capture_prompted_ = false;
    bool tab_capture_allowed_ = false;
  };

  std::map<extensions::ExtensionId, UserChoice> user_choice_cache_;
  std::map<extensions::ExtensionId, std::unique_ptr<ExtensionInstallPrompt>>
      extension_install_prompt_map_;
  TabCaptureAccessHandler tab_capture_access_handler_;

  DISALLOW_COPY_AND_ASSIGN(PublicSessionTabCaptureAccessHandler);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_PUBLIC_SESSION_TAB_CAPTURE_ACCESS_HANDLER_H_
