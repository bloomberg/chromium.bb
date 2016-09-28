// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/mojo/chrome_interface_registrar_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "content/public/browser/android/interface_registry_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/ChromeInterfaceRegistrar_jni.h"
#include "services/shell/public/cpp/interface_registry.h"

// static
void ChromeInterfaceRegistrarAndroid::ExposeInterfacesToFrame(
    shell::InterfaceRegistry* registry,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  // Happens when, for example, showing the malware interstitial page.
  if (!web_contents)
    return;

  Java_ChromeInterfaceRegistrar_exposeInterfacesToFrame(
      base::android::AttachCurrentThread(),
      content::InterfaceRegistryAndroid::Create(registry)->GetObj(),
      web_contents->GetJavaWebContents());
}
