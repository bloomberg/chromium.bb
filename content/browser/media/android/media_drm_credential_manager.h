// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_DRM_CREDENTIAL_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_DRM_CREDENTIAL_MANAGER_H_

#include <string>

#include "base/callback.h"

namespace media {
class MediaDrmBridge;
}

namespace content {

// This class manages the media DRM credentials on Android.
class MediaDrmCredentialManager {
 public:
  MediaDrmCredentialManager();
  ~MediaDrmCredentialManager();

  typedef base::Callback<void(bool)> ResetCredentialsCB;

  // Called to reset the DRM credentials.
  void ResetCredentials(const ResetCredentialsCB& callback);

 private:
  // Callback function passed to MediaDrmBridge. It is called when reset
  // completed.
  void OnResetCredentialsCompleted(const std::string& security_level,
                                   bool success);

  // Reset DRM credentials for a particular security level. Returns false if
  // we fail to create the MediaDrmBridge, or true otherwise.
  bool ResetCredentialsInternal(const std::string& security_level);

  // The MediaDrmBridge object used to perform the credential reset.
  scoped_ptr<media::MediaDrmBridge> media_drm_bridge_;

  // The callback provided by the caller.
  ResetCredentialsCB reset_credentials_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmCredentialManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_DRM_CREDENTIAL_MANAGER_H_
