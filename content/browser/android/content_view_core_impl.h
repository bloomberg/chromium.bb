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
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
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
                      bool hardware_accelerated,
                      bool take_ownership_of_web_contents,
                      WebContents* web_contents);

  // ContentViewCore overrides
  virtual void Destroy(JNIEnv* env, jobject obj) OVERRIDE;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Notifies the ContentViewCore that items were selected in the currently
  // showing select popup.
  void SelectPopupMenuItems(JNIEnv* env, jobject obj, jintArray indices);

  void LoadUrl(
      JNIEnv* env, jobject obj,
      jstring url,
      jint load_url_type,
      jint transition_type,
      jint ua_override_option,
      jstring extra_headers,
      jbyteArray post_data,
      jstring base_url_for_data_url,
      jstring virtual_url_for_data_url);
  base::android::ScopedJavaLocalRef<jstring> GetURL(JNIEnv* env, jobject) const;
  base::android::ScopedJavaLocalRef<jstring> GetTitle(
      JNIEnv* env, jobject obj) const;
  jboolean IsIncognito(JNIEnv* env, jobject obj);
  jboolean Crashed(JNIEnv* env, jobject obj) const { return tab_crashed_; }
  jboolean TouchEvent(JNIEnv* env,
                      jobject obj,
                      jlong time_ms,
                      jint type,
                      jobjectArray pts);
  void ScrollBegin(JNIEnv* env, jobject obj, jlong time_ms, jint x, jint y);
  void ScrollEnd(JNIEnv* env, jobject obj, jlong time_ms);
  void ScrollBy(JNIEnv* env, jobject obj, jlong time_ms, jint x, jint y,
                jint dx, jint dy);
  void FlingStart(JNIEnv* env,
                  jobject obj,
                  jlong time_ms,
                  jint x,
                  jint y,
                  jint vx,
                  jint vy);
  void FlingCancel(JNIEnv* env, jobject obj, jlong time_ms);
  void SingleTap(JNIEnv* env,
                 jobject obj,
                 jlong time_ms,
                 jint x,
                 jint y,
                 jboolean disambiguation_popup_tap);
  void ShowPressState(JNIEnv* env, jobject obj, jlong time_ms, jint x, jint y);
  void DoubleTap(JNIEnv* env, jobject obj, jlong time_ms, jint x, jint y) ;
  void LongPress(JNIEnv* env,
                 jobject obj,
                 jlong time_ms,
                 jint x,
                 jint y,
                 jboolean disambiguation_popup_tap);
  void PinchBegin(JNIEnv* env, jobject obj, jlong time_ms, jint x, jint y);
  void PinchEnd(JNIEnv* env, jobject obj, jlong time_ms);
  void PinchBy(JNIEnv* env,
               jobject obj,
               jlong time_ms,
               jint x,
               jint y,
               jfloat delta);
  virtual void SelectBetweenCoordinates(JNIEnv* env, jobject obj,
                                        jint x1, jint y1,
                                        jint x2, jint y2) OVERRIDE;
  jboolean CanGoBack(JNIEnv* env, jobject obj);
  jboolean CanGoForward(JNIEnv* env, jobject obj);
  jboolean CanGoToOffset(JNIEnv* env, jobject obj, jint offset);
  void GoBack(JNIEnv* env, jobject obj);
  void GoForward(JNIEnv* env, jobject obj);
  void GoToOffset(JNIEnv* env, jobject obj, jint offset);
  void StopLoading(JNIEnv* env, jobject obj);
  void Reload(JNIEnv* env, jobject obj);
  jboolean NeedsReload(JNIEnv* env, jobject obj);
  void ClearHistory(JNIEnv* env, jobject obj);
  void SetClient(JNIEnv* env, jobject obj, jobject jclient);
  jint EvaluateJavaScript(JNIEnv* env, jobject obj, jstring script);
  virtual int GetNativeImeAdapter(JNIEnv* env, jobject obj) OVERRIDE;
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() OVERRIDE;
  void AddJavascriptInterface(JNIEnv* env,
                              jobject obj,
                              jobject object,
                              jstring name,
                              jboolean require_annotation);
  void RemoveJavascriptInterface(JNIEnv* env, jobject obj, jstring name);
  int GetNavigationHistory(JNIEnv* env, jobject obj, jobject context);

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
  void ImeUpdateAdapter(int native_ime_adapter, int text_input_type,
                        const std::string& text,
                        int selection_start, int selection_end,
                        int composition_start, int composition_end,
                        bool show_ime_if_needed);
  void SetTitle(const string16& title);

  bool HasFocus();
  void ConfirmTouchEvent(bool handled);
  void DidSetNeedTouchEvents(bool need_touch_events);
  void OnSelectionChanged(const std::string& text);
  void OnSelectionBoundsChanged(int startx,
                                int starty,
                                base::i18n::TextDirection start_dir,
                                int endx,
                                int endy,
                                base::i18n::TextDirection end_dir);

  // Called when page loading begins.
  void DidStartLoading();
  void StartContentIntent(const GURL& content_url);

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  gfx::Rect GetBounds() const;

  WebContents* web_contents() const { return web_contents_; }

  virtual void LoadUrl(NavigationController::LoadURLParams& params) OVERRIDE;

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

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid();

  int GetTouchPadding();

  void SendGestureEvent(WebKit::WebInputEvent::Type type, long time_ms,
                        int x, int y);

  struct JavaObject;
  JavaObject* java_object_;

  // A weak reference to the Java ContentViewCore object.
  JavaObjectWeakGlobalRef java_ref_;

  NotificationRegistrar notification_registrar_;

  // Reference to the current WebContents used to determine how and what to
  // display in the ContentViewCore.
  WebContentsImpl* web_contents_;
  bool owns_web_contents_;

  // We only set this to be the delegate of the web_contents if we own it.
  scoped_ptr<ContentViewClient> content_view_client_;

  // Whether the renderer backing this ContentViewCore has crashed.
  bool tab_crashed_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewCoreImpl);
};

bool RegisterContentViewCore(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_
