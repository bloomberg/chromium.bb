// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/omnibox/omnibox_view_util.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/omnibox/autocomplete_input.h"
#include "jni/OmniboxViewUtil_jni.h"

// static
jstring SanitizeTextForPaste(JNIEnv* env, jclass clazz, jstring jtext) {
  base::string16 pasted_text(
      base::android::ConvertJavaStringToUTF16(env, jtext));
  pasted_text = OmniboxView::SanitizeTextForPaste(pasted_text);
  return base::android::ConvertUTF16ToJavaString(env, pasted_text).Release();
}

// static
jintArray ParseForEmphasizeComponents(JNIEnv* env,
                                      jclass clazz,
                                      jobject jprofile,
                                      jstring jtext) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);

  base::string16 text(
        base::android::ConvertJavaStringToUTF16(env, jtext));

  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      text, ChromeAutocompleteSchemeClassifier(profile), &scheme, &host);

  int emphasize_values[] =
      {scheme.begin, scheme.len, host.begin, host.len};
  return base::android::ToJavaIntArray(env, emphasize_values, 4).Release();
}

// static
bool OmniboxViewUtil::RegisterOmniboxViewUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
