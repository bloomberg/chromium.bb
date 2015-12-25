// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/context_menu_helper.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/context_menu_params.h"
#include "jni/ContextMenuHelper_jni.h"
#include "jni/ContextMenuParams_jni.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/point.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ContextMenuHelper);

namespace {

const int kShareImageMaxWidth = 2048;
const int kShareImageMaxHeight = 2048;

}  // namespace

ContextMenuHelper::ContextMenuHelper(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env,
      Java_ContextMenuHelper_create(env, reinterpret_cast<long>(this)).obj());
  DCHECK(!java_obj_.is_null());
}

ContextMenuHelper::~ContextMenuHelper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_destroy(env, java_obj_.obj());
}

bool ContextMenuHelper::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::FromWebContents(web_contents_);

  if (!content_view_core)
    return false;

  base::android::ScopedJavaLocalRef<jobject> jcontent_view_core(
      content_view_core->GetJavaObject());

  if (jcontent_view_core.is_null())
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  context_menu_params_ = params;
  render_frame_id_ = render_frame_host->GetRoutingID();
  render_process_id_ = render_frame_host->GetProcess()->GetID();

  return Java_ContextMenuHelper_showContextMenu(
      env, java_obj_.obj(), jcontent_view_core.obj(),
      ContextMenuHelper::CreateJavaContextMenuParams(params).obj());
}

void ContextMenuHelper::SetPopulator(jobject jpopulator) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_setPopulator(env, java_obj_.obj(), jpopulator);
}

base::android::ScopedJavaLocalRef<jobject>
ContextMenuHelper::CreateJavaContextMenuParams(
    const content::ContextMenuParams& params) {
  GURL sanitizedReferrer = (params.frame_url.is_empty() ?
      params.page_url : params.frame_url).GetAsReferrer();

  std::map<std::string, std::string>::const_iterator it =
      params.properties.find(data_reduction_proxy::chrome_proxy_header());
  bool image_was_fetched_lo_fi =
      it != params.properties.end() &&
      it->second == data_reduction_proxy::chrome_proxy_lo_fi_directive();

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jmenu_info =
      ContextMenuParamsAndroid::Java_ContextMenuParams_create(
          env,
          params.media_type,
          ConvertUTF8ToJavaString(env, params.page_url.spec()).obj(),
          ConvertUTF8ToJavaString(env, params.link_url.spec()).obj(),
          ConvertUTF16ToJavaString(env, params.link_text).obj(),
          ConvertUTF8ToJavaString(env, params.unfiltered_link_url.spec()).obj(),
          ConvertUTF8ToJavaString(env, params.src_url.spec()).obj(),
          ConvertUTF16ToJavaString(env, params.title_text).obj(),
          image_was_fetched_lo_fi,
          ConvertUTF8ToJavaString(env, sanitizedReferrer.spec()).obj(),
          params.referrer_policy);

  return jmenu_info;
}

void ContextMenuHelper::OnStartDownload(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jboolean jis_link,
                                        const JavaParamRef<jstring>& jheaders) {
  std::string headers(ConvertJavaStringToUTF8(env, jheaders));
  content::DownloadControllerAndroid::Get()->StartContextMenuDownload(
      context_menu_params_,
      web_contents_,
      jis_link,
      headers);
}

void ContextMenuHelper::SearchForImage(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  CoreTabHelper::FromWebContents(web_contents_)->SearchByImageInNewTab(
      render_frame_host, context_menu_params_.src_url);
}

void ContextMenuHelper::ShareImage(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  CoreTabHelper::FromWebContents(web_contents_)->
      RequestThumbnailForContextNode(
          render_frame_host,
          0,
          gfx::Size(kShareImageMaxWidth, kShareImageMaxHeight),
          base::Bind(&ContextMenuHelper::OnShareImage,
                     base::Unretained(this)));
}

void ContextMenuHelper::OnShareImage(const std::string& thumbnail_data,
                                     const gfx::Size& original_size) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::FromWebContents(web_contents_);
  if (!content_view_core)
    return;

  base::android::ScopedJavaLocalRef<jobject> jwindow_android(
      content_view_core->GetWindowAndroid()->GetJavaObject());

  if (jwindow_android.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> j_bytes =
      base::android::ToJavaByteArray(
          env, reinterpret_cast<const uint8_t*>(thumbnail_data.data()),
          thumbnail_data.length());

  Java_ContextMenuHelper_onShareImageReceived(
      env,
      java_obj_.obj(),
      jwindow_android.obj(),
      j_bytes.obj());
}

bool RegisterContextMenuHelper(JNIEnv* env) {
  return RegisterNativesImpl(env) &&
         ContextMenuParamsAndroid::RegisterNativesImpl(env);
}
