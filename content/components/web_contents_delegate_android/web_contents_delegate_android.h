// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_WEB_CONTENTS_DELEGATE_ANDROID_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_COMPONENT_WEB_CONTENTS_DELEGATE_ANDROID_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

namespace content {
class JavaScriptDialogCreator;
class RenderViewHost;
class WebContents;
class WebContentsObserver;
struct NativeWebKeyboardEvent;

enum WebContentsDelegateLogLevel {
  // Equivalent of WebCore::WebConsoleMessage::LevelTip.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_TIP = 0,
  // Equivalent of WebCore::WebConsoleMessage::LevelLog.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG = 1,
  // Equivalent of WebCore::WebConsoleMessage::LevelWarning.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING = 2,
  // Equivalent of WebCore::WebConsoleMessage::LevelError.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR = 3,
};


// Native underpinnings of WebContentsDelegateAndroid.java. Provides a default
// delegate for WebContents to forward calls to the java peer. The embedding
// application may subclass and override methods on either the C++ or Java side
// as required.
class WebContentsDelegateAndroid : public WebContentsDelegate {
 public:
  WebContentsDelegateAndroid(JNIEnv* env, jobject obj);
  virtual ~WebContentsDelegateAndroid();

  // Binds this WebContentsDelegateAndroid to the passed WebContents instance,
  // such that when that WebContents is destroyed, this
  // WebContentsDelegateAndroid instance will be destroyed too.
  void SetOwnerWebContents(WebContents* contents);

  // Overridden from WebContentsDelegate:
  virtual WebContents* OpenURLFromTab(
      WebContents* source,
      const OpenURLParams& params) OVERRIDE;

  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* source, int color_chooser_id,
      SkColor color) OVERRIDE;
  virtual void NavigationStateChanged(const WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(WebContents* source,
                              WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void ActivateContents(WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(WebContents* contents) OVERRIDE;
  virtual void LoadingStateChanged(WebContents* source) OVERRIDE;
  virtual void LoadProgressChanged(WebContents* source,
                                   double load_progress) OVERRIDE;
  virtual void CloseContents(WebContents* source) OVERRIDE;
  virtual void MoveContents(WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual bool AddMessageToConsole(WebContents* source,
                                   int32 level,
                                   const string16& message,
                                   int32 line_no,
                                   const string16& source_id) OVERRIDE;
  // TODO(merge): WARNING! method no longer available on the base class.
  // See http://crbug.com/149477
  virtual void URLStarredChanged(WebContents* source, bool starred);
  virtual void UpdateTargetURL(WebContents* source,
                               int32 page_id,
                               const GURL& url) OVERRIDE;
  virtual void HandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool TakeFocus(WebContents* source, bool reverse) OVERRIDE;

  virtual void ShowRepostFormWarningDialog(WebContents* source) OVERRIDE;

  virtual void ToggleFullscreenModeForTab(content::WebContents* web_contents,
                                          bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const OVERRIDE;

 protected:
  base::android::ScopedJavaLocalRef<jobject> GetJavaDelegate(JNIEnv* env) const;

 private:
  // We depend on the java side user of WebContentDelegateAndroid to hold a
  // strong reference to that object as long as they want to receive callbacks
  // on it. Using a weak ref here allows it to be correctly GCed.
  JavaObjectWeakGlobalRef weak_java_delegate_;
};

bool RegisterWebContentsDelegateAndroid(JNIEnv* env);

}  // namespace content

#endif  // CHROME_BROWSER_COMPONENT_WEB_CONTENTS_DELEGATE_ANDROID_WEB_CONTENTS_DELEGATE_ANDROID_H_
