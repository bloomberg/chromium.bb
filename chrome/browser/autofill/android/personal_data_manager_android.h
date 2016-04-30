// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {

// Android wrapper of the PersonalDataManager which provides access from the
// Java layer. Note that on Android, there's only a single profile, and
// therefore a single instance of this wrapper.
class PersonalDataManagerAndroid : public PersonalDataManagerObserver {
 public:
  PersonalDataManagerAndroid(JNIEnv* env, jobject obj);

  // These functions act on "web profiles" aka "LOCAL_PROFILE" profiles.
  // -------------------------

  // Returns the number of web and auxiliary profiles.
  jint GetProfileCount(JNIEnv* unused_env,
                       const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the profile as indexed by |index| in the PersonalDataManager's
  // |GetProfiles()| collection.
  base::android::ScopedJavaLocalRef<jobject> GetProfileByIndex(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      jint index);

  // Returns the profile with the specified |jguid|, or NULL if there is no
  // profile with the specified |jguid|. Both web and auxiliary profiles may
  // be returned.
  base::android::ScopedJavaLocalRef<jobject> GetProfileByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Adds or modifies a profile.  If |jguid| is an empty string, we are creating
  // a new profile.  Else we are updating an existing profile.  Always returns
  // the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile);

  // Gets the labels for all known profiles. These labels are useful for
  // distinguishing the profiles from one another.
  //
  // The labels never contain the full name and include at least 2 fields.
  //
  // If |address_only| is true, then such fields as phone number and email
  // address are also omitted, but all fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabels(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      bool address_only);

  // These functions act on local credit cards.
  // --------------------

  // Returns the number of credit cards.
  jint GetCreditCardCount(
      JNIEnv* unused_env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the credit card as indexed by |index| in the PersonalDataManager's
  // |GetCreditCards()| collection.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByIndex(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      jint index);

  // Returns the credit card with the specified |jguid|, or NULL if there is
  // no credit card with the specified |jguid|.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Adds or modifies a credit card.  If |jguid| is an empty string, we are
  // creating a new profile.  Else we are updating an existing profile.  Always
  // returns the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetCreditCard(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Removes the profile or credit card represented by |jguid|.
  void RemoveByGUID(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& unused_obj,
                    const base::android::JavaParamRef<jstring>& jguid);

  // Resets the given unmasked card back to the masked state.
  void ClearUnmaskedCache(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Gets the card CVC and unmasks the card (if it's masked).
  void GetFullCardForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jstring>& jguid,
      const base::android::JavaParamRef<jobject>& jdelegate);

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // Registers the JNI bindings for this class.
  static bool Register(JNIEnv* env);

 private:
  ~PersonalDataManagerAndroid() override;

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the PersonalDataManager for the main profile.
  PersonalDataManager* personal_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
