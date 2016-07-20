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

  // Returns the GUIDs of all profiles.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the GUIDs of the profiles to suggest to the user. See
  // PersonalDataManager::GetProfilesToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

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
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Gets the labels for the profiles to suggest to the user. These labels are
  // useful for distinguishing the profiles from one another.
  //
  // The labels never contain the full name, email address, or phone numbers.
  // All other fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the label of the given profile for PaymentRequest. This label does
  // not contain the full name or the email address. All other fields are
  // included in the label.
  base::android::ScopedJavaLocalRef<jstring> GetAddressLabelForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile);

  // These functions act on local credit cards.
  // --------------------

  // Returns the GUIDs of all the credit cards.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the GUIDs of the credit cards to suggest to the user. See
  // PersonalDataManager::GetCreditCardsToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the credit card with the specified |jguid|, or NULL if there is
  // no credit card with the specified |jguid|.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns a credit card with the specified |jcard_number|. This is used for
  // determining the card's obfuscated number, issuer icon, and type in one go.
  // This function does not interact with the autofill table on disk, so can be
  // used for cards that are not saved.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardForNumber(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jcard_number);

  // Adds or modifies a local credit card.  If |jguid| is an empty string, we
  // are creating a new card.  Else we are updating an existing profile.  Always
  // returns the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetCreditCard(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Updates the billing address of a server credit card with GUID |jguid|.
  void UpdateServerCardBillingAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid,
      const base::android::JavaParamRef<jstring>& jbilling_address_id);

  // Returns the card type according to PaymentRequest spec, or an empty string
  // if the given card number is not valid.
  base::android::ScopedJavaLocalRef<jstring> GetBasicCardPaymentTypeIfValid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jcard_number);

  // Adds a server credit card. Used only in tests.
  void AddServerCreditCardForTest(
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

  // Gets the card CVC and expiration date (if it's expired). If the card is
  // masked, unmasks it. If the user has entered new expiration date, the new
  // date is saved on disk.
  //
  // The full card details are sent to the delegate.
  void GetFullCardForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jstring>& jguid,
      const base::android::JavaParamRef<jobject>& jdelegate);

  // Gets the card CVC and expiration date (if it's expired) for a card that's
  // temporary and not stored on disk. No unmasking is performed. If the user
  // has entered new expiration date, the new date is not saved on disk, because
  // this card is temporary.
  //
  // The card number, name on card, and expiration date are used for UI display
  // and are sent to the delegate after user confirms their CVC.
  void GetFullTemporaryCardForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jstring>& jcard_number,
      const base::android::JavaParamRef<jstring>& jname_on_card,
      const base::android::JavaParamRef<jstring>& jexpiration_month,
      const base::android::JavaParamRef<jstring>& jexpiration_year,
      const base::android::JavaParamRef<jobject>& jdelegate);

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // Registers the JNI bindings for this class.
  static bool Register(JNIEnv* env);

  // These functions act on the usage stats of local profiles and credit cards.
  // --------------------

  // Records the use and log usage metrics for the profile associated with the
  // |jguid|. Increments the use count of the profile and sets its use date to
  // the current time.
  void RecordAndLogProfileUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Sets the use count and use date of the profile associated to the |jguid|.
  // Both |count| and |date| should be non-negative. |date| represents an
  // absolute point in coordinated universal time (UTC) represented as
  // microseconds since the Windows epoch. For more details see the comment
  // header in time.h.
  void SetProfileUseStatsForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid,
      jint count,
      jint date);

  // Returns the use count of the profile associated to the |jguid|.
  jint GetProfileUseCountForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the use date of the profile associated to the |jguid|. It
  // represents an absolute point in coordinated universal time (UTC)
  // represented as microseconds since the Windows epoch. For more details see
  // the comment header in time.h.
  jlong GetProfileUseDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Records the use and log usage metrics for the credit card associated with
  // the |jguid|. Increments the use count of the credit card and sets its use
  // date to the current time.
  void RecordAndLogCreditCardUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Sets the use count and use date of the credit card associated to the
  // |jguid|. Both |count| and |date| should be non-negative. |date| represents
  // an absolute point in coordinated universal time (UTC) represented as
  // microseconds since the Windows epoch. For more details see the comment
  // header in time.h.
  void SetCreditCardUseStatsForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid,
      jint count,
      jint date);

  // Returns the use count of the credit card associated to the |jguid|.
  jint GetCreditCardUseCountForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the use date of the credit card associated to the |jguid|. It
  // represents an absolute point in coordinated universal time (UTC)
  // represented as microseconds since the Windows epoch. For more details see
  // the comment header in time.h.
  jlong GetCreditCardUseDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the current date represented as an absolute point in coordinated
  // universal time (UTC) represented as microseconds since the Unix epoch. For
  // more details see the comment header in time.h
  jlong GetCurrentDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

 private:
  ~PersonalDataManagerAndroid() override;

  // Returns the GUIDs of the |profiles| passed as parameter.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDs(
      JNIEnv* env,
      const std::vector<AutofillProfile*>& profiles);

  // Returns the GUIDs of the |credit_cards| passed as parameter.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDs(
      JNIEnv* env,
      const std::vector<CreditCard*>& credit_cards);

  // Gets the labels for the |profiles| passed as parameters. These labels are
  // useful for distinguishing the profiles from one another.
  //
  // The labels never contain the full name and include at least 2 fields.
  //
  // If |address_only| is true, then such fields as phone number, and email
  // address are also omitted, but all other fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabels(
      JNIEnv* env,
      bool address_only,
      std::vector<AutofillProfile*> profiles);

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the PersonalDataManager for the main profile.
  PersonalDataManager* personal_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
