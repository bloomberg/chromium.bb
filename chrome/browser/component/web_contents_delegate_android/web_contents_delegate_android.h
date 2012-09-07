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
class DownloadItem;
class JavaScriptDialogCreator;
class RenderViewHost;
class WebContents;
class WebContentsObserver;
struct NativeWebKeyboardEvent;
}

namespace web_contents_delegate_android {

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
class WebContentsDelegateAndroid : public content::WebContentsDelegate {
 public:
  WebContentsDelegateAndroid(JNIEnv* env, jobject obj);
  virtual ~WebContentsDelegateAndroid();

  // Binds this WebContentsDelegateAndroid to the passed WebContents instance,
  // such that when that WebContents is destroyed, this
  // WebContentsDelegateAndroid instance will be destroyed too.
  void SetOwnerWebContents(content::WebContents* contents);

  // Overridden from WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual bool ShouldIgnoreNavigation(
      content::WebContents* source,
      const GURL& url,
      const content::Referrer& referrer,
      WindowOpenDisposition disposition,
      content::PageTransition transition_type) OVERRIDE;
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void ActivateContents(content::WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(content::WebContents* contents) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;
  virtual void LoadProgressChanged(content::WebContents* source,
                                   double load_progress) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual bool AddMessageToConsole(content::WebContents* source,
                                   int32 level,
                                   const string16& message,
                                   int32 line_no,
                                   const string16& source_id) OVERRIDE;
  // TODO(merge): WARNING! method no longer available on the base class.
  // See http://b/issue?id=5862108
  virtual void URLStarredChanged(content::WebContents* source, bool starred);
  virtual void UpdateTargetURL(content::WebContents* source,
                               int32 page_id,
                               const GURL& url) OVERRIDE;
  virtual bool CanDownload(content::RenderViewHost* source,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual void OnStartDownload(content::WebContents* source,
                               content::DownloadItem* download) OVERRIDE;
  virtual bool ShouldOverrideLoading(const GURL& url) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual content::JavaScriptDialogCreator* GetJavaScriptDialogCreator()
      OVERRIDE;
  virtual bool TakeFocus(content::WebContents* source, bool reverse) OVERRIDE;

  void SetJavaScriptDialogCreator(
      content::JavaScriptDialogCreator* javascript_dialog_creator) {
    javascript_dialog_creator_ = javascript_dialog_creator;
  }

 protected:
  base::android::ScopedJavaLocalRef<jobject> GetJavaDelegate(JNIEnv* env) const;

 private:
  // We depend on the java side user of WebContentDelegateAndroid to hold a
  // strong reference to that object as long as they want to receive callbacks
  // on it. Using a weak ref here allows it to be correctly GCed.
  JavaObjectWeakGlobalRef weak_java_delegate_;

  // The object responsible for creating JavaScript dialogs.
  content::JavaScriptDialogCreator* javascript_dialog_creator_;
};

bool RegisterWebContentsDelegateAndroid(JNIEnv* env);

}  // namespace web_contents_delegate_android

#endif  // CHROME_BROWSER_COMPONENT_WEB_CONTENTS_DELEGATE_ANDROID_WEB_CONTENTS_DELEGATE_ANDROID_H_
