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
#include "media/base/browser_cdm.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/media_export.h"
#include "media/cdm/player_tracker_impl.h"
#include "url/gurl.h"

class GURL;

namespace media {

class MediaPlayerManager;

// This class provides DRM services for android EME implementation.
class MEDIA_EXPORT MediaDrmBridge : public BrowserCdm {
 public:
  // TODO(ddorwin): These are specific to Widevine. http://crbug.com/459400
  enum SecurityLevel {
    SECURITY_LEVEL_NONE = 0,
    SECURITY_LEVEL_1 = 1,
    SECURITY_LEVEL_3 = 3,
  };

  typedef base::Callback<void(bool)> ResetCredentialsCB;

  ~MediaDrmBridge() override;

  // Checks whether MediaDRM is available.
  // All other static methods check IsAvailable() internally. There's no need
  // to check IsAvailable() explicitly before calling them.
  static bool IsAvailable();

  // Checks whether |key_system| is supported.
  static bool IsKeySystemSupported(const std::string& key_system);

  // Returns the list of the platform-supported key system names that
  // are not handled by Chrome explicitly.
  static std::vector<std::string> GetPlatformKeySystemNames();

  // Checks whether |key_system| is supported with |container_mime_type|.
  // |container_mime_type| must not be empty.
  static bool IsKeySystemSupportedWithType(
      const std::string& key_system,
      const std::string& container_mime_type);

  static bool RegisterMediaDrmBridge(JNIEnv* env);

  // Returns a MediaDrmBridge instance if |key_system| is supported, or a NULL
  // pointer otherwise.
  // TODO(xhwang): Is it okay not to update session expiration info?
  static scoped_ptr<MediaDrmBridge> Create(
      const std::string& key_system,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionErrorCB& session_error_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);

  // Returns a MediaDrmBridge instance if |key_system| is supported, or a NULL
  // otherwise. No session callbacks are provided. This is used when we need to
  // use MediaDrmBridge without creating any sessions.
  static scoped_ptr<MediaDrmBridge> CreateWithoutSessionSupport(
      const std::string& key_system);

  // Returns true if |security_level| is successfully set, or false otherwise.
  // Call this function right after Create() and before any other calls.
  // Note:
  // - If this function is not called, the default security level of the device
  //   will be used.
  // - It's recommended to call this function only once on a MediaDrmBridge
  //   object. Calling this function multiples times may cause errors.
  bool SetSecurityLevel(SecurityLevel security_level);

  // MediaKeys (via BrowserCdm) implementation.
  void SetServerCertificate(
      const uint8* certificate_data,
      int certificate_data_length,
      scoped_ptr<media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      const std::string& init_data_type,
      const uint8* init_data,
      int init_data_length,
      scoped_ptr<media::NewSessionCdmPromise> promise) override;
  void LoadSession(SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const uint8* response,
                     int response_length,
                     scoped_ptr<media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    scoped_ptr<media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     scoped_ptr<media::SimpleCdmPromise> promise) override;
  CdmContext* GetCdmContext() override;

  // PlayerTracker (via BrowserCdm) implementation.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) override;
  void UnregisterPlayer(int registration_id) override;

  // Returns a MediaCrypto object if it's already created. Returns a null object
  // otherwise.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Sets callback which will be called when MediaCrypto is ready.
  // If |closure| is null, previously set callback will be cleared.
  void SetMediaCryptoReadyCB(const base::Closure& closure);

  // Called after a MediaCrypto object is created.
  void OnMediaCryptoReady(JNIEnv* env, jobject j_media_drm);

  // Callbacks to resolve the promise for |promise_id|.
  void OnPromiseResolved(JNIEnv* env, jobject j_media_drm, jint j_promise_id);
  void OnPromiseResolvedWithSession(JNIEnv* env,
                                    jobject j_media_drm,
                                    jint j_promise_id,
                                    jbyteArray j_session_id);

  // Callback to reject the promise for |promise_id| with |error_message|.
  // Note: No |system_error| is available from MediaDrm.
  // TODO(xhwang): Implement Exception code.
  void OnPromiseRejected(JNIEnv* env,
                         jobject j_media_drm,
                         jint j_promise_id,
                         jstring j_error_message);

  // Session event callbacks.
  // Note: Session expiration update is not supported by MediaDrm.

  void OnSessionMessage(JNIEnv* env,
                        jobject j_media_drm,
                        jbyteArray j_session_id,
                        jbyteArray j_message,
                        jstring j_legacy_destination_url);
  void OnSessionClosed(JNIEnv* env,
                       jobject j_media_drm,
                       jbyteArray j_session_id);

  // Note: Key ID is not available in MediaDrm, so only a generic |j_key_status|
  // and |has_additional_usable_key| are returned.
  void OnSessionKeysChange(JNIEnv* env,
                           jobject j_media_drm,
                           jbyteArray j_session_id,
                           bool has_additional_usable_key,
                           jint j_key_status);

  // Called by the CDM when an error occurred in session |j_session_id|
  // unrelated to one of the MediaKeys calls that accept a |promise|.
  // Note:
  // - This method is only for supporting prefixed EME API.
  // - This method will be ignored by unprefixed EME. All errors reported
  //   in this method should probably also be reported by one of other methods.
  void OnLegacySessionError(JNIEnv* env,
                            jobject j_media_drm,
                            jbyteArray j_session_id,
                            jstring j_error_message);

  // Reset the device credentials.
  void ResetDeviceCredentials(const ResetCredentialsCB& callback);

  // Called by the java object when credential reset is completed.
  void OnResetDeviceCredentialsCompleted(JNIEnv* env, jobject, bool success);

  // Helper function to determine whether a protected surface is needed for the
  // video playback.
  bool IsProtectedSurfaceRequired();

 private:
  MediaDrmBridge(const std::vector<uint8>& scheme_uuid,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const SessionErrorCB& session_error_cb,
                 const SessionKeysChangeCB& session_keys_change_cb);

  static bool IsSecureDecoderRequired(SecurityLevel security_level);

  // Get the security level of the media.
  SecurityLevel GetSecurityLevel();

  // UUID of the key system.
  std::vector<uint8> scheme_uuid_;

  // Java MediaDrm instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_drm_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionErrorCB session_error_cb_;
  SessionKeysChangeCB session_keys_change_cb_;

  base::Closure media_crypto_ready_cb_;

  ResetCredentialsCB reset_credentials_cb_;

  PlayerTrackerImpl player_tracker_;

  CdmPromiseAdapter cdm_promise_adapter_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
