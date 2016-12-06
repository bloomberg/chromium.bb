// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/context_menu_params.h"
#include "ui/gfx/geometry/size.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}

class ContextMenuHelper
    : public content::WebContentsUserData<ContextMenuHelper> {
 public:
  ~ContextMenuHelper() override;

  void ShowContextMenu(content::RenderFrameHost* render_frame_host,
                       const content::ContextMenuParams& params);

  void OnContextMenuClosed(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

  void SetPopulator(jobject jpopulator);

  // Methods called from Java via JNI ------------------------------------------
  void OnStartDownload(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jboolean jis_link,
                       jboolean jis_data_reduction_proxy_enabled);
  void SearchForImage(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void ShareImage(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  explicit ContextMenuHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContextMenuHelper>;

  static base::android::ScopedJavaLocalRef<jobject> CreateJavaContextMenuParams(
      const content::ContextMenuParams& params);

  void OnShareImage(const std::string& thumbnail_data,
                    const gfx::Size& original_size);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  content::WebContents* web_contents_;

  content::ContextMenuParams context_menu_params_;
  int render_frame_id_;
  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuHelper);
};

bool RegisterContextMenuHelper(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
