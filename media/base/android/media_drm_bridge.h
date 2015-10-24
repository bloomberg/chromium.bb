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
#include "base/memory/weak_ptr.h"
#include "media/base/browser_cdm.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/media_export.h"
#include "media/cdm/player_tracker_impl.h"
#include "url/gurl.h"

class GURL;

namespace media {

class MediaDrmBridge;

using ScopedMediaDrmBridgePtr = scoped_ptr<MediaDrmBridge, BrowserCdmDeleter>;

// This class provides DRM services for android EME implementation.
class MEDIA_EXPORT MediaDrmBridge : public BrowserCdm {
 public:
  // TODO(ddorwin): These are specific to Widevine. http://crbug.com/459400
  enum SecurityLevel {
    SECURITY_LEVEL_NONE = 0,
    SECURITY_LEVEL_1 = 1,
    SECURITY_LEVEL_3 = 3,
  };

  using JavaObjectPtr = scoped_ptr<base::android::ScopedJavaGlobalRef<jobject>>;

  using ResetCredentialsCB = base::Callback<void(bool)>;

  // Notification called when MediaCrypto object is ready.
  // Parameters:
  // |media_crypto| - global reference to MediaCrypto object
  // |needs_protected_surface| - true if protected surface is required.
  using MediaCryptoReadyCB = base::Callback<void(JavaObjectPtr media_crypto,
                                                 bool needs_protected_surface)>;

  ~MediaDrmBridge() override;

  void DeleteOnCorrectThread() override;

  // Checks whether MediaDRM is available.
  // All other static methods check IsAvailable() internally. There's no need
  // to check IsAvailable() explicitly before calling them.
  static bool IsAvailable();

  static bool RegisterMediaDrmBridge(JNIEnv* env);

  // Checks whether |key_system| is supported.
  static bool IsKeySystemSupported(const std::string& key_system);

  // Checks whether |key_system| is supported with |container_mime_type|.
  // |container_mime_type| must not be empty.
  static bool IsKeySystemSupportedWithType(
      const std::string& key_system,
      const std::string& container_mime_type);

  // Returns the list of the platform-supported key system names that
  // are not handled by Chrome explicitly.
  static std::vector<std::string> GetPlatformKeySystemNames();

  // Returns a MediaDrmBridge instance if |key_system| is supported, or a NULL
  // pointer otherwise.
  // TODO(xhwang): Is it okay not to update session expiration info?
  static ScopedMediaDrmBridgePtr Create(
      const std::string& key_system,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const LegacySessionErrorCB& legacy_session_error_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);

  // Returns a MediaDrmBridge instance if |key_system| is supported, or a NULL
  // otherwise. No session callbacks are provided. This is used when we need to
  // use MediaDrmBridge without creating any sessions.
  static ScopedMediaDrmBridgePtr CreateWithoutSessionSupport(
      const std::string& key_system);

  // Returns a WeakPtr to be used on the |task_runner_|.
  base::WeakPtr<MediaDrmBridge> WeakPtr();

  // MediaKeys (via BrowserCdm) implementation.
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      scoped_ptr<media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      scoped_ptr<media::NewSessionCdmPromise> promise) override;
  void LoadSession(SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
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

  // Returns true if |security_level| is successfully set, or false otherwise.
  // Call this function right after Create() and before any other calls.
  // Note:
  // - If this function is not called, the default security level of the device
  //   will be used.
  // - It's recommended to call this function only once on a MediaDrmBridge
  //   object. Calling this function multiples times may cause errors.
  bool SetSecurityLevel(SecurityLevel security_level);

  // Helper function to determine whether a protected surface is needed for the
  // video playback.
  bool IsProtectedSurfaceRequired();

  // Reset the device credentials.
  void ResetDeviceCredentials(const ResetCredentialsCB& callback);

  // Helper functions to resolve promises.
  void ResolvePromise(uint32_t promise_id);
  void ResolvePromiseWithSession(uint32_t promise_id,
                                 const std::string& session_id);
  void RejectPromise(uint32_t promise_id, const std::string& error_message);

  // Returns a MediaCrypto object if it's already created. Returns a null object
  // otherwise.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Sets callback which will be called when MediaCrypto is ready. If
  // |media_crypto_ready_cb| is null, previously set callback will be cleared.
  void SetMediaCryptoReadyCB(const MediaCryptoReadyCB& media_crypto_ready_cb);

  // All the OnXxx functions below are called from Java. The implementation must
  // only do minimal work and then post tasks to avoid reentrancy issues.

  // Called by Java after a MediaCrypto object is created.
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

  // TODO(xhwang): Remove |j_legacy_destination_url| when prefixed EME support
  // is removed.
  void OnSessionMessage(JNIEnv* env,
                        jobject j_media_drm,
                        jbyteArray j_session_id,
                        jint j_message_type,
                        jbyteArray j_message,
                        jstring j_legacy_destination_url);
  void OnSessionClosed(JNIEnv* env,
                       jobject j_media_drm,
                       jbyteArray j_session_id);

  void OnSessionKeysChange(JNIEnv* env,
                           jobject j_media_drm,
                           jbyteArray j_session_id,
                           jobjectArray j_keys_info,
                           bool has_additional_usable_key);

  // |expiry_time_ms| is the new expiration time for the keys in the session.
  // The time is in milliseconds, relative to the Unix epoch. A time of 0
  // indicates that the keys never expire.
  void OnSessionExpirationUpdate(JNIEnv* env,
                                 jobject j_media_drm,
                                 jbyteArray j_session_id,
                                 jlong expiry_time_ms);

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

  // Called by the java object when credential reset is completed.
  void OnResetDeviceCredentialsCompleted(JNIEnv* env, jobject, bool success);

 private:
  MediaDrmBridge(const std::vector<uint8>& scheme_uuid,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const LegacySessionErrorCB& legacy_session_error_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb);

  static bool IsSecureDecoderRequired(SecurityLevel security_level);

  // Get the security level of the media.
  SecurityLevel GetSecurityLevel();

  // A helper method that calls a |player_tracker_| method on correct thread.
  void NotifyNewKeyOnCorrectThread();

  // A helper method that calculates the |media_crypto_ready_cb_| arguments and
  // run this callback.
  void NotifyMediaCryptoReady(const MediaCryptoReadyCB& cb);

  // UUID of the key system.
  std::vector<uint8> scheme_uuid_;

  // Java MediaDrm instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_drm_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  LegacySessionErrorCB legacy_session_error_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  MediaCryptoReadyCB media_crypto_ready_cb_;

  ResetCredentialsCB reset_credentials_cb_;

  // The |player_tracker_| must be accessed by one thread only. It is accessed
  // by the Media thread when |use_media_thread_| is true.
  PlayerTrackerImpl player_tracker_;

  scoped_ptr<CdmPromiseAdapter> cdm_promise_adapter_;

  // Default task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This flag is set when we use media thread for certain callbacks.
  const bool use_media_thread_;

  // NOTE: Weak pointers must be invalidated before all other member variables.

  // WeakPtrFactory to generate weak pointers to be used on the media thread.
  base::WeakPtrFactory<MediaDrmBridge> media_weak_factory_;

  // Default WeakPtrFactory.
  base::WeakPtrFactory<MediaDrmBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
