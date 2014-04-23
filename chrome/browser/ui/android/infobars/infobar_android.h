// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_ANDROID_H_

#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/infobars/core/infobar.h"

class InfoBarService;

namespace infobars {
class InfoBarDelegate;
}

class InfoBarAndroid : public infobars::InfoBar {
 public:

  // Make sure this set of values is aligned with the java constants defined in
  // InfoBar.java!
  enum ActionType {
    ACTION_NONE = 0,
    // Confirm infobar
    ACTION_OK = 1,
    ACTION_CANCEL = 2,
    // Translate infobar
    ACTION_TRANSLATE = 3,
    ACTION_TRANSLATE_SHOW_ORIGINAL = 4,
  };

  explicit InfoBarAndroid(scoped_ptr<infobars::InfoBarDelegate> delegate);
  virtual ~InfoBarAndroid();

  // InfoBar:
  virtual base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) = 0;

  void set_java_infobar(const base::android::JavaRef<jobject>& java_info_bar);
  bool HasSetJavaInfoBar() const;

  // Tells the Java-side counterpart of this InfoBar to point to the replacement
  // InfoBar instead of this one.
  void ReassignJavaInfoBar(InfoBarAndroid* replacement);

  virtual void OnLinkClicked(JNIEnv* env, jobject obj) {}
  void OnButtonClicked(JNIEnv* env,
                       jobject obj,
                       jint action,
                       jstring action_value);
  void OnCloseButtonClicked(JNIEnv* env, jobject obj);

  void CloseJavaInfoBar();

  // Maps from a Chromium ID (IDR_TRANSLATE) to a enum value that Java code can
  // translate into a Drawable ID using the ResourceId class.
  int GetEnumeratedIconId();

  // Acquire the java infobar from a different one.  This is used to do in-place
  // replacements.
  virtual void PassJavaInfoBar(InfoBarAndroid* source) {}

 protected:
  // Derived classes must implement this method to process the corresponding
  // action.
  virtual void ProcessButton(int action,
                             const std::string& action_value) = 0;
  void CloseInfoBar();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_info_bar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarAndroid);
};

// Registers the NativeInfoBar's native methods through JNI.
bool RegisterNativeInfoBar(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_ANDROID_H_
