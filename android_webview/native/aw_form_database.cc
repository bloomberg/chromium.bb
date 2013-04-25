// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_form_database.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/time.h"
#include "components/autofill/browser/webdata/autofill_webdata_service.h"
#include "jni/AwFormDatabase_jni.h"

namespace android_webview {

// static
void ClearFormData(JNIEnv*, jclass) {
  AwBrowserContext* context = AwContentBrowserClient::GetAwBrowserContext();
  DCHECK(context);

  autofill::AutofillWebDataService* service =
      autofill::AutofillWebDataService::FromBrowserContext(context).get();
  if (service == NULL) {
    LOG(WARNING) << "No webdata service found, ignoring ClearFormData";
    return;
  }

  base::Time begin;
  base::Time end = base::Time::Max();
  service->RemoveFormElementsAddedBetween(begin, end);
  service->RemoveAutofillDataModifiedBetween(begin, end);
}

bool RegisterFormDatabase(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

} // namespace android_webview
