// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

class GURL;

namespace media {

class MediaPlayerManager;

// This class provides DRM services for android EME implementation.
// TODO(qinmin): implement all the functions in this class.
class MEDIA_EXPORT MediaDrmBridge : public MediaKeys {
 public:
  enum SecurityLevel {
    SECURITY_LEVEL_NONE = 0,
    SECURITY_LEVEL_1 = 1,
    SECURITY_LEVEL_3 = 3,
  };

  typedef base::Callback<void(bool)> ResetCredentialsCB;

  virtual ~MediaDrmBridge();

  // Checks whether MediaDRM is available.
  // All other static methods check IsAvailable() internally. There's no need
  // to check IsAvailable() explicitly before calling them.
  static bool IsAvailable();

  static bool IsSecurityLevelSupported(const std::string& key_system,
                                       SecurityLevel security_level);

  // TODO(xhwang): The |container_mime_type| is not the same as contentType in
  // the EME spec. Revisit this once the spec issue with initData type is
  // resolved.
  static bool IsKeySystemSupportedWithType(
      const std::string& key_system,
      const std::string& container_mime_type);

  static bool IsSecureDecoderRequired(SecurityLevel security_level);

  static bool RegisterMediaDrmBridge(JNIEnv* env);

  // Returns a MediaDrmBridge instance if |key_system| is supported, or a NULL
  // pointer otherwise.
  static scoped_ptr<MediaDrmBridge> Create(int cdm_id,
                                           const std::string& key_system,
                                           const GURL& security_origin,
                                           MediaPlayerManager* manager);

  // Returns true if |security_level| is successfully set, or false otherwise.
  // Call this function right after Create() and before any other calls.
  // Note:
  // - If this function is not called, the default security level of the device
  //   will be used.
  // - It's recommended to call this function only once on a MediaDrmBridge
  //   object. Calling this function multiples times may cause errors.
  bool SetSecurityLevel(SecurityLevel security_level);

  // MediaKeys implementations.
  virtual bool CreateSession(uint32 session_id,
                             const std::string& content_type,
                             const uint8* init_data,
                             int init_data_length) OVERRIDE;
  virtual void LoadSession(uint32 session_id,
                           const std::string& web_session_id) OVERRIDE;
  virtual void UpdateSession(uint32 session_id,
                             const uint8* response,
                             int response_length) OVERRIDE;
  virtual void ReleaseSession(uint32 session_id) OVERRIDE;

  // Returns a MediaCrypto object if it's already created. Returns a null object
  // otherwise.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Sets callback which will be called when MediaCrypto is ready.
  // If |closure| is null, previously set callback will be cleared.
  void SetMediaCryptoReadyCB(const base::Closure& closure);

  // Called after a MediaCrypto object is created.
  void OnMediaCryptoReady(JNIEnv* env, jobject j_media_drm);

  // Callbacks for firing session events.
  void OnSessionCreated(JNIEnv* env,
                        jobject j_media_drm,
                        jint j_session_id,
                        jstring j_web_session_id);
  void OnSessionMessage(JNIEnv* env,
                        jobject j_media_drm,
                        jint j_session_id,
                        jbyteArray j_message,
                        jstring j_destination_url);
  void OnSessionReady(JNIEnv* env, jobject j_media_drm, jint j_session_id);
  void OnSessionClosed(JNIEnv* env, jobject j_media_drm, jint j_session_id);
  void OnSessionError(JNIEnv* env, jobject j_media_drm, jint j_session_id);

  // Reset the device credentials.
  void ResetDeviceCredentials(const ResetCredentialsCB& callback);

  // Called by the java object when credential reset is completed.
  void OnResetDeviceCredentialsCompleted(JNIEnv* env, jobject, bool success);

  // Helper function to determine whether a protected surface is needed for the
  // video playback.
  bool IsProtectedSurfaceRequired();

  int cdm_id() const { return cdm_id_; }

  const GURL& security_origin() const { return security_origin_; }

 private:
  MediaDrmBridge(int cdm_id,
                 const std::vector<uint8>& scheme_uuid,
                 const GURL& security_origin,
                 MediaPlayerManager* manager);

  // Get the security level of the media.
  SecurityLevel GetSecurityLevel();

  // ID of the CDM object.
  int cdm_id_;

  // UUID of the key system.
  std::vector<uint8> scheme_uuid_;

  // media stream's security origin.
  const GURL security_origin_;

  // Java MediaDrm instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_drm_;

  // Non-owned pointer.
  MediaPlayerManager* manager_;

  base::Closure media_crypto_ready_cb_;

  ResetCredentialsCB reset_credentials_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
