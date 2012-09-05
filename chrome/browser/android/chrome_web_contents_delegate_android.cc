// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_web_contents_delegate_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/find_bar/find_match_rects_details.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/file_chooser_params.h"
#include "jni/ChromeWebContentsDelegateAndroid_jni.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"

using base::android::AttachCurrentThread;
using base::android::GetClass;
using base::android::ScopedJavaLocalRef;
using content::FileChooserParams;
using content::WebContents;

namespace {

// Convenience method to create Android rects.
// RectType should be either gfx::Rect or gfx::RectF.
template <typename RectType>
ScopedJavaLocalRef<jobject> CreateAndroidRect(
    JNIEnv* env,
    const ScopedJavaLocalRef<jclass>& clazz,
    const jmethodID& constructor,
    const RectType& rect) {

  ScopedJavaLocalRef<jobject> rect_object(
      env,
      env->NewObject(clazz.obj(),
                     constructor,
                     rect.x(),
                     rect.y(),
                     rect.right(),
                     rect.bottom()));

  DCHECK(!rect_object.is_null());
  return rect_object;
}

}  // anonymous namespace

namespace chrome {
namespace android {

ChromeWebContentsDelegateAndroid::ChromeWebContentsDelegateAndroid(JNIEnv* env,
                                                                   jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

ChromeWebContentsDelegateAndroid::~ChromeWebContentsDelegateAndroid() {
  notification_registrar_.RemoveAll();
}

// Register native methods.
bool RegisterChromeWebContentsDelegateAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ChromeWebContentsDelegateAndroid::RunFileChooser(
    WebContents* web_contents,
    const FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void ChromeWebContentsDelegateAndroid::CloseContents(
    WebContents* web_contents) {
  // Prevent dangling registrations assigned to closed web contents.
  if (notification_registrar_.IsRegistered(this,
      chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
      content::Source<WebContents>(web_contents))) {
    notification_registrar_.Remove(this,
        chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
        content::Source<WebContents>(web_contents));
  }

  WebContentsDelegateAndroid::CloseContents(web_contents);
}

void ChromeWebContentsDelegateAndroid::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_FIND_RESULT_AVAILABLE:
      OnFindResultAvailable(
          content::Source<WebContents>(source).ptr(),
          content::Details<FindNotificationDetails>(details).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification: " << type;
      break;
  }
}

void ChromeWebContentsDelegateAndroid::FindReply(
    WebContents* web_contents,
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  if (!notification_registrar_.IsRegistered(this,
      chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
      content::Source<WebContents>(web_contents))) {
    notification_registrar_.Add(this,
        chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
        content::Source<WebContents>(web_contents));
  }

  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  tab_contents->find_tab_helper()->HandleFindReply(request_id,
                                                   number_of_matches,
                                                   selection_rect,
                                                   active_match_ordinal,
                                                   final_update);
}

void ChromeWebContentsDelegateAndroid::OnFindResultAvailable(
    WebContents* web_contents,
    const FindNotificationDetails* find_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  // Create the selection rect.
  ScopedJavaLocalRef<jclass> rect_clazz =
      GetClass(env, "android/graphics/Rect");

  jmethodID rect_constructor =
      GetMethodID(env, rect_clazz, "<init>", "(IIII)V");

  ScopedJavaLocalRef<jobject> selection_rect = CreateAndroidRect(
      env, rect_clazz, rect_constructor, find_result->selection_rect());

  // Create the details object.
  ScopedJavaLocalRef<jclass> details_clazz =
      GetClass(env, "org/chromium/chrome/browser/FindNotificationDetails");

  jmethodID details_constructor = GetMethodID(env, details_clazz, "<init>",
                                              "(ILandroid/graphics/Rect;IZ)V");

  ScopedJavaLocalRef<jobject> details_object(
      env,
      env->NewObject(details_clazz.obj(),
                     details_constructor,
                     find_result->number_of_matches(),
                     selection_rect.obj(),
                     find_result->active_match_ordinal(),
                     find_result->final_update()));
  DCHECK(!details_object.is_null());

  Java_ChromeWebContentsDelegateAndroid_onFindResultAvailable(
      env,
      obj.obj(),
      details_object.obj());
}

void ChromeWebContentsDelegateAndroid::FindMatchRectsReply(
    WebContents* web_contents,
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  FindMatchRectsDetails match_rects(version, rects, active_rect);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FIND_MATCH_RECTS_AVAILABLE,
      content::Source<WebContents>(web_contents),
      content::Details<FindMatchRectsDetails>(&match_rects));

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  // Create the rects.
  ScopedJavaLocalRef<jclass> rect_clazz =
      GetClass(env, "android/graphics/RectF");

  jmethodID rect_constructor =
      GetMethodID(env, rect_clazz, "<init>", "(FFFF)V");

  ScopedJavaLocalRef<jobjectArray> jrects(env, env->NewObjectArray(
      match_rects.rects().size(), rect_clazz.obj(), NULL));

  for (size_t i = 0; i < match_rects.rects().size(); ++i) {
    env->SetObjectArrayElement(
        jrects.obj(), i,
        CreateAndroidRect(env,
                          rect_clazz,
                          rect_constructor,
                          match_rects.rects()[i]).obj());
  }

  ScopedJavaLocalRef<jobject> jactive_rect = CreateAndroidRect(
      env, rect_clazz, rect_constructor, match_rects.active_rect());

  // Create the details object.
  ScopedJavaLocalRef<jclass> details_clazz =
      GetClass(env, "org/chromium/chrome/browser/FindMatchRectsDetails");

  jmethodID details_constructor = GetMethodID(env, details_clazz, "<init>",
      "(I[Landroid/graphics/RectF;Landroid/graphics/RectF;)V");

  ScopedJavaLocalRef<jobject> details_object(
      env,
      env->NewObject(details_clazz.obj(),
                     details_constructor,
                     match_rects.version(),
                     jrects.obj(),
                     jactive_rect.obj()));
  DCHECK(!details_object.is_null());

  Java_ChromeWebContentsDelegateAndroid_onFindMatchRectsAvailable(
      env,
      obj.obj(),
      details_object.obj());
}

}  // namespace android
}  // namespace chrome
