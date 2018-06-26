// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"
#include "jni/DisplayCutoutController_jni.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"

using base::android::JavaParamRef;

namespace chrome {

// static
void JNI_DisplayCutoutController_SetSafeAreaOnWebContents(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& j_contents_android,
    int top,
    int left,
    int bottom,
    int right) {
  content::WebContents* contents =
      content::WebContents::FromJavaWebContents(j_contents_android);
  if (!contents)
    return;

  content::RenderFrameHost* frame = contents->GetMainFrame();
  if (!frame)
    return;

  blink::AssociatedInterfaceProvider* provider =
      frame->GetRemoteAssociatedInterfaces();
  if (!provider)
    return;

  blink::mojom::DisplayCutoutClientAssociatedPtr client;
  provider->GetInterface(&client);
  client->SetSafeArea(
      blink::mojom::DisplayCutoutSafeArea::New(top, left, bottom, right));
}

}  // namespace chrome
