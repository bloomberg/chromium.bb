// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_ANDROID_SHELL_VIEW_H_
#define CONTENT_SHELL_ANDROID_SHELL_VIEW_H_

#include <jni.h>

#include "base/basictypes.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/jni_helper.h"
#include "content/public/browser/web_contents.h"

class TabContents;

namespace content {

// Native representation of the java ShellView.  Handles JNI communication
// between the two instances.
class ShellView {
 public:
  ShellView(JNIEnv* env, jobject obj);
  virtual ~ShellView();

  // Initializes the java components based on the tab contents.
  void InitFromTabContents(WebContents* tab_contents);

  // Registers the ShellView native methods.
  static bool Register(JNIEnv* env);

 private:
  JavaObjectWeakGlobalRef weak_java_shell_view_;

  DISALLOW_COPY_AND_ASSIGN(ShellView);
};

}  // namespace content

#endif  // CONTENT_SHELL_ANDROID_SHELL_VIEW_H_
