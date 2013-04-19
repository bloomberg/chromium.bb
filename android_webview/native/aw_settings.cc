// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_settings.h"

#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/native/aw_contents.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/AwSettings_jni.h"
#include "webkit/glue/webkit_glue.h"

namespace android_webview {

AwSettings::AwSettings(JNIEnv* env, jobject obj)
    : java_ref_(env, obj),
      initial_page_scale_percent_(0),
      text_zoom_percent_(100) {
}

AwSettings::~AwSettings() {
}

void AwSettings::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

AwRenderViewHostExt* AwSettings::GetAwRenderViewHostExt() {
  if (!web_contents()) return NULL;
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (!contents) return NULL;
  return contents->render_view_host_ext();
}

void AwSettings::ResetScrollAndScaleState(JNIEnv* env, jobject obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe) return;
  rvhe->ResetScrollAndScaleState();
}

void AwSettings::SetInitialPageScale(
    JNIEnv* env, jobject obj, jfloat page_scale_percent) {
  if (initial_page_scale_percent_ == page_scale_percent) return;
  initial_page_scale_percent_ = page_scale_percent;
  UpdateInitialPageScale();
}

void AwSettings::SetTextZoom(JNIEnv* env, jobject obj, jint text_zoom_percent) {
  if (text_zoom_percent_ == text_zoom_percent) return;
  text_zoom_percent_ = text_zoom_percent;
  UpdateTextZoom();
}

void AwSettings::SetWebContents(JNIEnv* env, jobject obj, jint jweb_contents) {
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(jweb_contents);
  Observe(web_contents);

  UpdateRenderViewHostExtSettings();
  if (web_contents->GetRenderViewHost()) {
    UpdateRenderViewHostSettings(web_contents->GetRenderViewHost());
  }
}

void AwSettings::UpdateInitialPageScale() {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe) return;
  if (initial_page_scale_percent_ == 0) {
    rvhe->SetInitialPageScale(-1);
  } else {
    rvhe->SetInitialPageScale(initial_page_scale_percent_ / 100.0f);
  }
}

void AwSettings::UpdateTextZoom() {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe) return;
  if (text_zoom_percent_ > 0) {
    rvhe->SetTextZoomLevel(webkit_glue::ZoomFactorToZoomLevel(
        text_zoom_percent_ / 100.0f));
  } else {
    // Use the default zoom level value when Text Autosizer is turned on.
    rvhe->SetTextZoomLevel(0);
  }
}

void AwSettings::UpdatePreferredSizeMode(
    content::RenderViewHost* render_view_host) {
  render_view_host->EnablePreferredSizeMode();
}

void AwSettings::UpdateRenderViewHostExtSettings() {
  UpdateInitialPageScale();
  UpdateTextZoom();
}

void AwSettings::UpdateRenderViewHostSettings(
    content::RenderViewHost* render_view_host) {
  UpdatePreferredSizeMode(render_view_host);
}

void AwSettings::RenderViewCreated(content::RenderViewHost* render_view_host) {
  // A single WebContents can normally have 0, 1 or 2 RenderViewHost instances
  // associated with it.
  // This is important since there is only one RenderViewHostExt instance per
  // WebContents (and not one RVHExt per RVH, as you might expect) and updating
  // settings via RVHExt only ever updates the 'current' RVH.
  // In android_webview we don't swap out the RVH on cross-site navigations, so
  // we shouldn't have to deal with the multiple RVH per WebContents case. That
  // in turn means that the newly created RVH is always the 'current' RVH
  // (since we only ever go from 0 to 1 RVH instances) and hence the DCHECK.
  DCHECK(web_contents()->GetRenderViewHost() == render_view_host);

  UpdateRenderViewHostExtSettings();
  UpdateRenderViewHostSettings(render_view_host);
}

static jint Init(JNIEnv* env,
                 jobject obj,
                 jint web_contents) {
  AwSettings* settings = new AwSettings(env, obj);
  settings->SetWebContents(env, obj, web_contents);
  return reinterpret_cast<jint>(settings);
}

bool RegisterAwSettings(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

}  // namespace android_webview
