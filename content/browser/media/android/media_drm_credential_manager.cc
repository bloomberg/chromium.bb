// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_drm_credential_manager.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "jni/MediaDrmCredentialManager_jni.h"
#include "media/base/android/media_drm_bridge.h"
#include "url/gurl.h"

using base::android::ScopedJavaGlobalRef;

namespace {

void MediaDrmCredentialManagerCallback(
    const ScopedJavaGlobalRef<jobject>& j_media_drm_credential_manager_callback,
    bool succeeded) {
  JNIEnv* env = base::android::AttachCurrentThread();
  content::Java_MediaDrmCredentialManagerCallback_onCredentialResetFinished(
      env, j_media_drm_credential_manager_callback.obj(), succeeded);
}

}  // namespace

namespace content {

// TODO(qinmin): Move the UUID definition to some common places.
static const uint8 kWidevineUuid[16] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };

MediaDrmCredentialManager::MediaDrmCredentialManager() {};

MediaDrmCredentialManager::~MediaDrmCredentialManager() {};

// static
MediaDrmCredentialManager* MediaDrmCredentialManager::GetInstance() {
  return Singleton<MediaDrmCredentialManager>::get();
}

void MediaDrmCredentialManager::ResetCredentials(
    const ResetCredentialsCB& reset_credentials_cb) {
  // Ignore reset request if one is already in progress.
  if (!reset_credentials_cb_.is_null())
    return;

  reset_credentials_cb_ = reset_credentials_cb;

  // First reset the L3 credential.
  if (!ResetCredentialsInternal(media::MediaDrmBridge::SECURITY_LEVEL_3)) {
    // TODO(qinmin): We should post a task instead.
    base::ResetAndReturn(&reset_credentials_cb_).Run(false);
  }
}

// static
void ResetCredentials(
    JNIEnv* env,
    jclass clazz,
    jobject j_media_drm_credential_manager_callback) {
  MediaDrmCredentialManager* media_drm_credential_manager =
      MediaDrmCredentialManager::GetInstance();

  ScopedJavaGlobalRef<jobject> j_scoped_media_drm_credential_manager_callback;
  j_scoped_media_drm_credential_manager_callback.Reset(
      env, j_media_drm_credential_manager_callback);

  MediaDrmCredentialManager::ResetCredentialsCB callback_runner =
      base::Bind(&MediaDrmCredentialManagerCallback,
                 j_scoped_media_drm_credential_manager_callback);

  media_drm_credential_manager->ResetCredentials(callback_runner);
}

void MediaDrmCredentialManager::OnResetCredentialsCompleted(
    SecurityLevel security_level, bool success) {
  if (security_level == media::MediaDrmBridge::SECURITY_LEVEL_3 && success) {
    if (ResetCredentialsInternal(media::MediaDrmBridge::SECURITY_LEVEL_1))
      return;
    success = false;
  }

  base::ResetAndReturn(&reset_credentials_cb_).Run(success);
  media_drm_bridge_.reset();
}

bool MediaDrmCredentialManager::ResetCredentialsInternal(
    SecurityLevel security_level) {
  std::vector<uint8> uuid(kWidevineUuid, kWidevineUuid + 16);
  media_drm_bridge_ = media::MediaDrmBridge::Create(0, uuid, GURL(), NULL);
  if (!media_drm_bridge_)
    return false;

  ResetCredentialsCB reset_credentials_cb =
      base::Bind(&MediaDrmCredentialManager::OnResetCredentialsCompleted,
                 base::Unretained(this), security_level);

  if (!media_drm_bridge_->SetSecurityLevel(security_level)) {
    // No need to reset credentials for unsupported |security_level|.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(reset_credentials_cb, true));
    return true;
  }

  media_drm_bridge_->ResetDeviceCredentials(reset_credentials_cb);
  return true;
}

// static
bool MediaDrmCredentialManager::RegisterMediaDrmCredentialManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
