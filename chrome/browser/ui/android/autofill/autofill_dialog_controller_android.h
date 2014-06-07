// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/core/browser/autofill_client.h"

class Profile;

namespace autofill {

// This class defines the form-filling host and JNI glue for the Java-side
// implementation of the requestAutocomplete dialog.
class AutofillDialogControllerAndroid : public AutofillDialogController {
 public:
  // Creates an instance of the AutofillDialogControllerAndroid.
  static base::WeakPtr<AutofillDialogController> Create(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const AutofillClient::ResultCallback& callback);

  virtual ~AutofillDialogControllerAndroid();

  // AutofillDialogController implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void TabActivated() OVERRIDE;

  // JNI bindings for Java-side AutofillDialogDelegate:
  void DialogCancel(JNIEnv* env, jobject obj);
  void DialogContinue(JNIEnv* env,
                      jobject obj,
                      jobject full_wallet,
                      jboolean last_used_choice_is_autofill,
                      jstring last_used_account_name,
                      jstring last_used_billing,
                      jstring last_used_shipping,
                      jstring last_used_credit_card);

  // Establishes JNI bindings.
  static bool RegisterAutofillDialogControllerAndroid(JNIEnv* env);

 private:
  AutofillDialogControllerAndroid(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const AutofillClient::ResultCallback& callback);

  const AutofillMetrics& GetMetricLogger() const {
    return metric_logger_;
  }

  // Logs metrics when the dialog is submitted.
  void LogOnFinishSubmitMetrics();

  // Logs metrics when the dialog is canceled.
  void LogOnCancelMetrics();

  // The |profile| for |contents_|.
  Profile* const profile_;

  // The WebContents where the Autocomplete/Checkout action originated.
  content::WebContents* const contents_;

  // For logging UMA metrics.
  const AutofillMetrics metric_logger_;
  base::Time dialog_shown_timestamp_;
  AutofillMetrics::DialogInitialUserStateMetric initial_user_state_;

  FormStructure form_structure_;

  // Whether the URL visible to the user when this dialog was requested to be
  // invoked is the same as |source_url_|.
  bool invoked_from_same_origin_;

  // The URL of the invoking site.
  GURL source_url_;

  // The callback via which we return the collected data.
  AutofillClient::ResultCallback callback_;

  // Whether |form_structure_| has asked for any details that would indicate
  // we should show a shipping section.
  bool cares_about_shipping_;

  base::WeakPtrFactory<AutofillDialogControllerAndroid>
      weak_ptr_factory_;

  // Whether the latency to display to the UI was logged to UMA yet.
  bool was_ui_latency_logged_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_ANDROID_H_
