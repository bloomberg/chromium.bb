// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ANDROID_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

namespace send_tab_to_self {
class SendTabToSelfModel;

// The delegate to fetch SendTabToSelf information and persist new
// SendTabToSelf entries. The class is owned by the SendTabToSelf Java
// counterpart and lives for the duration of the life of that class.
class SendTabToSelfAndroidBridge {
 public:
  SendTabToSelfAndroidBridge(JNIEnv* env,
                             const base::android::JavaRef<jobject>& obj,
                             const base::android::JavaRef<jobject>& j_profile);

  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>&);

  // Populates a list of GUIDs in the model.
  void GetAllGuids(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_guid_list_obj);

  // Returns the entry associated with a GUID. May return nullptr if none is
  // found.
  base::android::ScopedJavaLocalRef<jobject> GetEntryByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_guid);

  // Adds a new entry with the specified parameters. Returns the persisted
  // version which contains additional information such as GUID.
  base::android::ScopedJavaLocalRef<jobject> AddEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_url,
      const base::android::JavaParamRef<jstring>& j_title);

  // Deletes all entries in the model.
  void DeleteAllEntries(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

 protected:
  ~SendTabToSelfAndroidBridge() {}
  // Set during the constructor and owned by the SendTabToSelfSyncServiceFactory
  // is based off the KeyedServiceFactory which lives for the length of the
  // profile. SendTabToSelf is not supported for the InCognito profile.
  SendTabToSelfModel* send_tab_to_self_model_;

 private:
  JavaObjectWeakGlobalRef weak_java_ref_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfAndroidBridge);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ANDROID_BRIDGE_H_
