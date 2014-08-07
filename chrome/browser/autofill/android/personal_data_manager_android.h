// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {

// Android wrapper of the PersonalDataManager which provides access from the
// Java layer. Note that on Android, there's only a single profile, and
// therefore a single instance of this wrapper.
class PersonalDataManagerAndroid : public PersonalDataManagerObserver {
 public:
  PersonalDataManagerAndroid(JNIEnv* env, jobject obj);

  // Regular Autofill Profiles
  // -------------------------

  // Returns the number of web and auxiliary profiles.
  jint GetProfileCount(JNIEnv* unused_env, jobject unused_obj);

  // Returns the profile as indexed by |index| in the PersonalDataManager's
  // |GetProfiles()| collection.
  base::android::ScopedJavaLocalRef<jobject> GetProfileByIndex(
      JNIEnv* env,
      jobject unused_obj,
      jint index);

  // Returns the profile with the specified |jguid|, or NULL if there is no
  // profile with the specified |jguid|. Both web and auxiliary profiles may
  // be returned.
  base::android::ScopedJavaLocalRef<jobject> GetProfileByGUID(
      JNIEnv* env,
      jobject unused_obj,
      jstring jguid);

  // Adds or modifies a profile.  If |jguid| is an empty string, we are creating
  // a new profile.  Else we are updating an existing profile.  Always returns
  // the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetProfile(JNIEnv* env,
                                                        jobject unused_obj,
                                                        jobject jprofile);

  // Credit Card Profiles
  // --------------------

  // Returns the number of credit cards.
  jint GetCreditCardCount(JNIEnv* unused_env, jobject unused_obj);

  // Returns the credit card as indexed by |index| in the PersonalDataManager's
  // |GetCreditCards()| collection.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByIndex(
      JNIEnv* env,
      jobject unused_obj,
      jint index);

  // Returns the credit card with the specified |jguid|, or NULL if there is
  // no credit card with the specified |jguid|.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByGUID(
      JNIEnv* env,
      jobject unused_obj,
      jstring jguid);

  // Adds or modifies a credit card.  If |jguid| is an empty string, we are
  // creating a new profile.  Else we are updating an existing profile.  Always
  // returns the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetCreditCard(
      JNIEnv* env,
      jobject unused_obj,
      jobject jcard);

  // Gets the labels for all known profiles. These labels are useful for
  // distinguishing the profiles from one another.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabels(
      JNIEnv* env,
      jobject unused_obj);

  // Removes the profile or credit card represented by |jguid|.
  void RemoveByGUID(JNIEnv* env, jobject unused_obj, jstring jguid);

  // PersonalDataManagerObserver:
  virtual void OnPersonalDataChanged() OVERRIDE;

  // Registers the JNI bindings for this class.
  static bool Register(JNIEnv* env);

 private:
  virtual ~PersonalDataManagerAndroid();

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the PersonalDataManager for the main profile.
  PersonalDataManager* personal_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
