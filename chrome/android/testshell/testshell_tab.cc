// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/testshell/testshell_tab.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/TestShellTab_jni.h"
#include "ui/base/android/window_android.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using chrome::android::ChromeWebContentsDelegateAndroid;
using content::WebContents;
using ui::WindowAndroid;

TestShellTab::TestShellTab(JNIEnv* env,
                           jobject obj)
    : TabAndroid(env, obj) {
}

TestShellTab::~TestShellTab() {
}

void TestShellTab::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void TestShellTab::OnReceivedHttpAuthRequest(jobject auth_handler,
                                             const string16& host,
                                             const string16& realm) {
  NOTIMPLEMENTED();
}

void TestShellTab::AddShortcutToBookmark(
    const GURL& url, const string16& title, const SkBitmap& skbitmap,
    int r_value, int g_value, int b_value) {
  NOTIMPLEMENTED();
}

void TestShellTab::EditBookmark(int64 node_id,
                                const base::string16& node_title,
                                bool is_folder,
                                bool is_partner_bookmark) {
  NOTIMPLEMENTED();
}

bool TestShellTab::ShouldWelcomePageLinkToTermsOfService() {
  NOTIMPLEMENTED();
  return false;
}

void TestShellTab::OnNewTabPageReady() {
  NOTIMPLEMENTED();
}

void TestShellTab::HandlePopupNavigation(chrome::NavigateParams* params) {
  NOTIMPLEMENTED();
}

bool TestShellTab::RegisterTestShellTab(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ScopedJavaLocalRef<jstring> TestShellTab::FixupUrl(JNIEnv* env,
                                                   jobject obj,
                                                   jstring url) {
  GURL fixed_url(URLFixerUpper::FixupURL(ConvertJavaStringToUTF8(env, url),
                                         std::string()));

  std::string fixed_spec;
  if (fixed_url.is_valid())
    fixed_spec = fixed_url.spec();

  return ConvertUTF8ToJavaString(env, fixed_spec);
}

static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<intptr_t>(new TestShellTab(env, obj));
}
