// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_base_android_impl.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "jni/TabBase_jni.h"
#include "ui/gfx/android/window_android.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using chrome::android::ChromeWebContentsDelegateAndroid;
using content::WebContents;
using ui::WindowAndroid;

TabBaseAndroidImpl::TabBaseAndroidImpl(JNIEnv* env,
                                       jobject obj,
                                       WebContents* web_contents,
                                       WindowAndroid* window_android)
    : web_contents_(web_contents) {
  InitTabHelpers(web_contents);
  WindowAndroidHelper::FromWebContents(web_contents)->
      SetWindowAndroid(window_android);
}

TabBaseAndroidImpl::~TabBaseAndroidImpl() {
}

void TabBaseAndroidImpl::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

WebContents* TabBaseAndroidImpl::GetWebContents() {
  return web_contents_.get();
}

browser_sync::SyncedTabDelegate* TabBaseAndroidImpl::GetSyncedTabDelegate() {
  NOTIMPLEMENTED();
  return NULL;
}

void TabBaseAndroidImpl::OnReceivedHttpAuthRequest(jobject auth_handler,
                                                   const string16& host,
                                                   const string16& realm) {
  NOTIMPLEMENTED();
}

void TabBaseAndroidImpl::ShowContextMenu(
    const content::ContextMenuParams& params) {
  NOTIMPLEMENTED();
}

void TabBaseAndroidImpl::ShowCustomContextMenu(
    const content::ContextMenuParams& params,
    const base::Callback<void(int)>& callback) {
  NOTIMPLEMENTED();
}

void TabBaseAndroidImpl::AddShortcutToBookmark(
    const GURL& url, const string16& title, const SkBitmap& skbitmap,
    int r_value, int g_value, int b_value) {
  NOTIMPLEMENTED();
}

void TabBaseAndroidImpl::EditBookmark(int64 node_id, bool is_folder) {
  NOTIMPLEMENTED();
}

void TabBaseAndroidImpl::RunExternalProtocolDialog(const GURL& url) {
  NOTIMPLEMENTED();
}

bool TabBaseAndroidImpl::RegisterTabBaseAndroidImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void TabBaseAndroidImpl::InitWebContentsDelegate(
    JNIEnv* env,
    jobject obj,
    jobject web_contents_delegate) {
  web_contents_delegate_.reset(
      new ChromeWebContentsDelegateAndroid(env, web_contents_delegate));
  web_contents_->SetDelegate(web_contents_delegate_.get());
}

ScopedJavaLocalRef<jstring> TabBaseAndroidImpl::FixupUrl(JNIEnv* env,
                                                         jobject obj,
                                                         jstring url) {
  GURL fixed_url(URLFixerUpper::FixupURL(ConvertJavaStringToUTF8(env, url),
                                         std::string()));

  std::string fixed_spec;
  if (fixed_url.is_valid())
    fixed_spec = fixed_url.spec();

  return ConvertUTF8ToJavaString(env, fixed_spec);
}

static jint Init(JNIEnv* env,
                 jobject obj,
                 jint web_contents_ptr,
                 jint window_android_ptr) {
  TabBaseAndroidImpl* tab = new TabBaseAndroidImpl(
      env,
      obj,
      reinterpret_cast<WebContents*>(web_contents_ptr),
      reinterpret_cast<WindowAndroid*>(window_android_ptr));
  return reinterpret_cast<jint>(tab);
}
