// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_

#include <vector>

#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/notification_observer.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

struct WebMenuItem;

namespace content {
class ContentViewClient;
class RenderWidgetHostViewAndroid;

// TODO(jrg): this is a shell.  Upstream the rest.
class ContentViewCoreImpl : public ContentViewCore,
                            public NotificationObserver {
 public:
  ContentViewCoreImpl(JNIEnv* env,
                      jobject obj,
                      WebContents* web_contents);
  virtual void Destroy(JNIEnv* env, jobject obj);

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Notifies the ContentViewCore that items were selected in the currently
  // showing select popup.
  void SelectPopupMenuItems(JNIEnv* env, jobject obj, jintArray indices);

  void LoadUrlWithoutUrlSanitization(JNIEnv* env,
                                     jobject,
                                     jstring jurl,
                                     int page_transition);
  void LoadUrlWithoutUrlSanitizationWithUserAgentOverride(
      JNIEnv* env,
      jobject,
      jstring jurl,
      int page_transition,
      jstring user_agent_override);
  base::android::ScopedJavaLocalRef<jstring> GetURL(JNIEnv* env, jobject) const;
  base::android::ScopedJavaLocalRef<jstring> GetTitle(
      JNIEnv* env, jobject obj) const;
  jboolean IsIncognito(JNIEnv* env, jobject obj);
  jboolean Crashed(JNIEnv* env, jobject obj) const { return tab_crashed_; }
  jboolean CanGoBack(JNIEnv* env, jobject obj);
  jboolean CanGoForward(JNIEnv* env, jobject obj);
  jboolean CanGoToOffset(JNIEnv* env, jobject obj, jint offset);
  void GoBack(JNIEnv* env, jobject obj);
  void GoForward(JNIEnv* env, jobject obj);
  void GoToOffset(JNIEnv* env, jobject obj, jint offset);
  jdouble GetLoadProgress(JNIEnv* env, jobject obj) const;
  void StopLoading(JNIEnv* env, jobject obj);
  void Reload(JNIEnv* env, jobject obj);
  jboolean NeedsReload(JNIEnv* env, jobject obj);
  void ClearHistory(JNIEnv* env, jobject obj);
  void SetClient(JNIEnv* env, jobject obj, jobject jclient);

  // --------------------------------------------------------------------------
  // Public methods that call to Java via JNI
  // --------------------------------------------------------------------------

  // Creates a popup menu with |items|.
  // |multiple| defines if it should support multi-select.
  // If not |multiple|, |selected_item| sets the initially selected item.
  // Otherwise, item's "checked" flag selects it.
  void ShowSelectPopupMenu(const std::vector<WebMenuItem>& items,
                           int selected_item,
                           bool multiple);

  void OnTabCrashed(const base::ProcessHandle handle);
  void SetTitle(const string16& title);

  bool HasFocus();
  void OnSelectionChanged(const std::string& text);
  void OnSelectionBoundsChanged(int startx,
                                int starty,
                                base::i18n::TextDirection start_dir,
                                int endx,
                                int endy,
                                base::i18n::TextDirection end_dir);

  // Called when page loading begins.
  void DidStartLoading();

  void OnAcceleratedCompositingStateChange(RenderWidgetHostViewAndroid* rwhva,
                                           bool activated,
                                           bool force);
  virtual void StartContentIntent(const GURL& content_url) OVERRIDE;

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  gfx::Rect GetBounds() const;

  WebContents* web_contents() const { return web_contents_; }
  void LoadUrl(const GURL& url, int page_transition);
  void LoadUrlWithUserAgentOverride(
      const GURL& url,
      int page_transition,
      const std::string& user_agent_override);

 private:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // --------------------------------------------------------------------------
  // Private methods that call to Java via JNI
  // --------------------------------------------------------------------------
  virtual ~ContentViewCoreImpl();

  // --------------------------------------------------------------------------
  // Other private methods and data
  // --------------------------------------------------------------------------

  void InitJNI(JNIEnv* env, jobject obj);

  void PostLoadUrl(const GURL& url);

  struct JavaObject;
  JavaObject* java_object_;

  // Reference to the current WebContents used to determine how and what to
  // display in the ContentViewCore.
  WebContents* web_contents_;

  // We only set this to be the delegate of the web_contents if we own it.
  scoped_ptr<ContentViewClient> content_view_client_;

  // Whether the renderer backing this ContentViewCore has crashed.
  bool tab_crashed_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewCoreImpl);
};

bool RegisterContentViewCore(JNIEnv* env);

};  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_
