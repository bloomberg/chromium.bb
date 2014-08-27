// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browser_version.h"

#include "base/android/jni_string.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/grit/locale_settings.h"
#include "jni/BrowserVersion_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF8ToJavaString;

static jboolean IsOfficialBuild(JNIEnv* env, jclass /* clazz */) {
  return chrome::VersionInfo().IsOfficialBuild();
}

static jstring GetTermsOfServiceHtml(JNIEnv* env, jclass /* clazz */) {
  return ConvertUTF8ToJavaString(env,
      l10n_util::GetStringUTF8(IDS_TERMS_HTML)).Release();
}

bool RegisterBrowserVersion(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
