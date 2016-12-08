// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PUBLIC_SESSION_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_PUBLIC_SESSION_MEDIA_ACCESS_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/media/extension_media_access_handler.h"
#include "chrome/browser/media/media_access_handler.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/extension_id.h"

// MediaAccessHandler for extension capturing requests in Public Sessions. This
// class is implemented as a wrapper around ExtensionMediaAccessHandler. It
// allows for finer access control to audioCapture/videoCapture manifest
// permission features inside of Public Sessions.
//
// In Public Sessions, apps and extensions are force-installed by admin policy
// so the user does not get a chance to review the permissions for these apps.
// This is not acceptable from a security/privacy standpoint, so when an app
// uses the capture APIs for the first time, we show the user a dialog where
// they can choose whether to allow the extension access to camera and/or
// microphone. Note: camera and microphone are used through audioCapture and
// videoCapture manifest permissions which are limited to platform apps only.
class PublicSessionMediaAccessHandler : public MediaAccessHandler {
 public:
  PublicSessionMediaAccessHandler();
  ~PublicSessionMediaAccessHandler() override;

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
  // inside of ExtensionMediaAccessHandler.
  void ChainHandleRequest(content::WebContents* web_contents,
                          const content::MediaStreamRequest& request,
                          const content::MediaResponseCallback& callback,
                          const extensions::Extension* extension);

  // Function used to resolve user decision regarding allowing audio/video.
  void ResolvePermissionPrompt(content::WebContents* web_contents,
                               const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback,
                               const extensions::Extension* extension,
                               ExtensionInstallPrompt::Result prompt_result);

  // Class used to cache user choice regarding allowing audio/video capture.
  class UserChoice {
   public:
    // Helper function for checking if audio/video is allowed by user choice.
    bool IsAllowed(content::MediaStreamType type) const;
    // Helper function which returns true if audio/video wasn't prompted yet.
    bool NeedsPrompting(content::MediaStreamType type) const;
    void Set(content::MediaStreamType type, bool allowed);
    void SetPrompted(content::MediaStreamType type);

   private:
    bool audio_prompted_ = false;
    bool audio_allowed_ = false;
    bool video_prompted_ = false;
    bool video_allowed_ = false;
  };

  std::map<extensions::ExtensionId, UserChoice> user_choice_cache_;
  std::map<extensions::ExtensionId, std::unique_ptr<ExtensionInstallPrompt>>
      extension_install_prompt_map_;
  ExtensionMediaAccessHandler extension_media_access_handler_;

  DISALLOW_COPY_AND_ASSIGN(PublicSessionMediaAccessHandler);
};

#endif  // CHROME_BROWSER_MEDIA_PUBLIC_SESSION_MEDIA_ACCESS_HANDLER_H_
