// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents.h"

#include "android_webview/native/aw_browser_dependency_factory.h"
#include "android_webview/native/aw_contents_container.h"
#include "android_webview/native/aw_web_contents_delegate.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/supports_user_data.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/AwContents_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using content::ContentViewCore;
using content::WebContents;

namespace android_webview {

namespace {

const void* kAwContentsUserDataKey = &kAwContentsUserDataKey;

class AwContentsUserData : public base::SupportsUserData::Data {
 public:
  AwContentsUserData(AwContents* ptr) : contents_(ptr) {}
  AwContents* get() { return contents_; }

 private:
  AwContents* contents_;
};

}  // namespace

// static
AwContents* AwContents::FromWebContents(content::WebContents* web_contents) {
  if (web_contents) {
    AwContentsUserData* data = reinterpret_cast<AwContentsUserData*>(
              web_contents->GetUserData(kAwContentsUserDataKey));
    if (data) return data->get();
  }
  return NULL;
}

AwContents::AwContents(JNIEnv* env,
                       jobject obj,
                       jobject web_contents_delegate,
                       bool private_browsing)
    : java_ref_(env, obj),
      web_contents_delegate_(
          new AwWebContentsDelegate(env, web_contents_delegate)) {
  android_webview::AwBrowserDependencyFactory* dependency_factory =
      android_webview::AwBrowserDependencyFactory::GetInstance();
  content::WebContents* web_contents =
      dependency_factory->CreateWebContents(private_browsing);
  contents_container_.reset(dependency_factory->CreateContentsContainer(
      web_contents));
  web_contents->SetDelegate(web_contents_delegate_.get());
  web_contents_delegate_->SetJavaScriptDialogCreator(
      dependency_factory->GetJavaScriptDialogCreator());
  web_contents->SetUserData(kAwContentsUserDataKey,
                            new AwContentsUserData(this));
}

AwContents::~AwContents() {
  content::WebContents* web_contents = contents_container_->GetWebContents();
  DCHECK(AwContents::FromWebContents(web_contents) == this);
  web_contents->RemoveUserData(kAwContentsUserDataKey);
}

jint AwContents::GetWebContents(JNIEnv* env, jobject obj) {
  return reinterpret_cast<jint>(contents_container_->GetWebContents());
}

void AwContents::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

static jint Init(JNIEnv* env,
                 jobject obj,
                 jobject web_contents_delegate,
                 jboolean private_browsing) {
  AwContents* tab = new AwContents(env, obj, web_contents_delegate,
                                   private_browsing);
  return reinterpret_cast<jint>(tab);
}

bool RegisterAwContents(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}


}  // namespace android_webview
