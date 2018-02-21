// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_CONNECTION_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_CONNECTION_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/content_input_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace vr {

class KeyboardEdit;

class VrInputConnection {
 public:
  explicit VrInputConnection(content::WebContents* web_contents);
  ~VrInputConnection();

  base::WeakPtr<VrInputConnection> GetWeakPtr();

  void OnKeyboardEdit(const std::vector<vr::KeyboardEdit>& edits);
  void RequestTextState(vr::TextStateUpdateCallback callback);
  void UpdateTextState(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jstring jtext);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  vr::TextStateUpdateCallback text_state_update_callback_;

  base::WeakPtrFactory<VrInputConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrInputConnection);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_CONNECTION_H_
