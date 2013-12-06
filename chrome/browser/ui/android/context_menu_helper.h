// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/context_menu_params.h"

namespace content {
struct ContextMenuParams;
class WebContents;
}

class ContextMenuHelper
    : public content::WebContentsUserData<ContextMenuHelper> {
 public:
  virtual ~ContextMenuHelper();

  void ShowContextMenu(const content::ContextMenuParams& params);

  void ShowCustomContextMenu(
      const content::ContextMenuParams& params,
      const base::Callback<void(int)>& callback);

  void SetPopulator(jobject jpopulator);

  // Methods called from Java via JNI ------------------------------------------

  void OnCustomItemSelected(JNIEnv* env, jobject obj, jint action);
  void OnStartDownload(JNIEnv* env, jobject obj, jboolean jis_link);

 private:
  explicit ContextMenuHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContextMenuHelper>;

  static base::android::ScopedJavaLocalRef<jobject> CreateJavaContextMenuParams(
      const content::ContextMenuParams& params);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  content::WebContents* web_contents_;

  base::Callback<void(int)> context_menu_callback_;
  content::ContextMenuParams context_menu_params_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuHelper);
};

bool RegisterContextMenuHelper(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
