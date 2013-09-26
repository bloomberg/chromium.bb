// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_CONTAINER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_CONTAINER_ANDROID_H_

#include <map>
#include <string>

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/infobars/infobar_container.h"

class InfoBarAndroid;

namespace content {
class WebContents;
}

class InfoBarContainerAndroid : public InfoBarContainer {
 public:
  InfoBarContainerAndroid(JNIEnv* env,
                          jobject infobar_container,
                          jobject auto_login_delegate);
  void Destroy(JNIEnv* env, jobject obj);

  JavaObjectWeakGlobalRef auto_login_delegate() const {
    return weak_java_auto_login_delegate_;
  }

  JavaObjectWeakGlobalRef java_container() const {
    return weak_java_infobar_container_;
  }

  // The prerender is swapping the web contents.
// TODO(miguelg) Move this out of infobar container.
  void OnWebContentsReplaced(content::WebContents* old_web_contents,
                             content::WebContents* new_web_contents);

 private:
  virtual ~InfoBarContainerAndroid() OVERRIDE;

  // InfobarContainer:
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar,
                                          size_t position) OVERRIDE;
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) OVERRIDE;
  virtual void PlatformSpecificReplaceInfoBar(InfoBar* old_infobar,
                                              InfoBar* new_infobar)  OVERRIDE;

  // Create the Java equivalent of |android_bar| and add it to the java
  // container.
  void AttachJavaInfoBar(InfoBarAndroid* android_bar);

  // We're owned by the java infobar, need to use a weak ref so it can destroy
  // us.
  JavaObjectWeakGlobalRef weak_java_infobar_container_;
  JavaObjectWeakGlobalRef weak_java_auto_login_delegate_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerAndroid);
};

// Registers the InfoBarContainer's native methods through JNI.
bool RegisterInfoBarContainer(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_CONTAINER_ANDROID_H_
