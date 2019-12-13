// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service_proxy_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SharingServiceProxy_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_source.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "components/sync_device_info/device_info.h"

void JNI_SharingServiceProxy_InitSharingService(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  SharingService* service = SharingServiceFactory::GetForBrowserContext(
      ProfileAndroid::FromProfileAndroid(j_profile));
  DCHECK(service);
}

SharingServiceProxyAndroid::SharingServiceProxyAndroid(
    SharingService* sharing_service)
    : sharing_service_(sharing_service) {
  DCHECK(sharing_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SharingServiceProxy_onProxyCreated(env,
                                          reinterpret_cast<intptr_t>(this));
}

SharingServiceProxyAndroid::~SharingServiceProxyAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SharingServiceProxy_onProxyDestroyed(env);
}

void SharingServiceProxyAndroid::SendSharedClipboardMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_guid,
    const jlong j_last_updated_timestamp_millis,
    const base::android::JavaParamRef<jstring>& j_text,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  std::string guid = base::android::ConvertJavaStringToUTF8(env, j_guid);
  DCHECK(!guid.empty());

  std::string text = base::android::ConvertJavaStringToUTF8(env, j_text);
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(std::move(text));

  syncer::DeviceInfo device(
      guid,
      /*client_name=*/std::string(),
      /*chrome_version=*/std::string(),
      /*sync_user_agent=*/std::string(), sync_pb::SyncEnums::TYPE_UNSET,
      /*signin_scoped_device_id=*/std::string(), base::SysInfo::HardwareInfo(),
      base::Time::FromJavaTime(j_last_updated_timestamp_millis),
      /*send_tab_to_self_receiving_enabled=*/false,
      /*sharing_info=*/base::nullopt);
  auto callback =
      base::BindOnce(base::android::RunIntCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable));
  sharing_service_->SendMessageToDevice(
      device, kSendMessageTimeout, std::move(sharing_message),
      base::BindOnce(
          [](base::OnceCallback<void(int)> callback,
             SharingSendMessageResult result,
             std::unique_ptr<chrome_browser_sharing::ResponseMessage>
                 response) {
            std::move(callback).Run(static_cast<int>(result));
          },
          std::move(callback)));
}

void SharingServiceProxyAndroid::GetDeviceCandidates(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_device_info,
    jint j_required_feature) {
  auto device_candidates = sharing_service_->GetDeviceCandidates(
      static_cast<sync_pb::SharingSpecificFields::EnabledFeatures>(
          j_required_feature));
  for (const auto& device_info : device_candidates) {
    Java_SharingServiceProxy_createDeviceInfoAndAppendToList(
        env, j_device_info,
        base::android::ConvertUTF8ToJavaString(env, device_info->guid()),
        base::android::ConvertUTF8ToJavaString(env, device_info->client_name()),
        device_info->device_type(),
        device_info->last_updated_timestamp().ToJavaTime());
  }
}

void SharingServiceProxyAndroid::AddDeviceCandidatesInitializedObserver(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  sharing_service_->GetDeviceSource()->AddReadyCallback(
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable)));
}
