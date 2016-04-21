// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/mojo/chrome_service_registrar_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "content/public/browser/android/service_registry_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/ChromeServiceRegistrar_jni.h"

// static
bool ChromeServiceRegistrarAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void ChromeServiceRegistrarAndroid::RegisterRenderFrameMojoServices(
    content::ServiceRegistry* registry,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  // Happens when, for example, showing the malware interstitial page.
  if (!web_contents)
    return;

  Java_ChromeServiceRegistrar_registerRenderFrameMojoServices(
      base::android::AttachCurrentThread(),
      content::ServiceRegistryAndroid::Create(registry)->GetObj().obj(),
      web_contents->GetJavaWebContents().obj());
}
