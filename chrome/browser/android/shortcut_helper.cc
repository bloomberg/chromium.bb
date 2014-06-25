// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/ShortcutHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

ShortcutBuilder::ShortcutBuilder(content::WebContents* web_contents,
                                 const base::string16& title,
                                 int launcher_large_icon_size)
    : launcher_large_icon_size_(launcher_large_icon_size),
      shortcut_type_(BOOKMARK) {
  Observe(web_contents);
  url_ = web_contents->GetURL();
  if (title.length() > 0)
    title_ = title;
  else
    title_ = web_contents->GetTitle();

  // Send a message to the renderer to retrieve information about the page.
  Send(new ChromeViewMsg_RetrieveWebappInformation(routing_id(), url_));
}

void ShortcutBuilder::OnDidRetrieveWebappInformation(
    bool success,
    bool is_mobile_webapp_capable,
    bool is_apple_mobile_webapp_capable,
    const GURL& expected_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  Observe(NULL);

  if (!success) {
    LOG(ERROR) << "Failed to parse webpage.";
    Destroy();
    return;
  } else if (expected_url != url_) {
    LOG(ERROR) << "Unexpected URL returned.";
    Destroy();
    return;
  }

  if (is_apple_mobile_webapp_capable && !is_mobile_webapp_capable) {
    shortcut_type_ = APP_SHORTCUT_APPLE;
  } else if (is_apple_mobile_webapp_capable || is_mobile_webapp_capable) {
    shortcut_type_ = APP_SHORTCUT;
  } else {
    shortcut_type_ = BOOKMARK;
  }

  // Grab the best, largest icon we can find to represent this bookmark.
  // TODO(dfalcantara): Try combining with the new BookmarksHandler once its
  //                    rewrite is further along.
  std::vector<int> icon_types;
  icon_types.push_back(favicon_base::FAVICON);
  icon_types.push_back(favicon_base::TOUCH_PRECOMPOSED_ICON |
                       favicon_base::TOUCH_ICON);
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);

  // Using favicon if its size is not smaller than platform required size,
  // otherwise using the largest icon among all avaliable icons.
  int threshold_to_get_any_largest_icon = launcher_large_icon_size_ - 1;
  favicon_service->GetLargestRawFaviconForPageURL(url_, icon_types,
      threshold_to_get_any_largest_icon,
      base::Bind(&ShortcutBuilder::FinishAddingShortcut,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

void ShortcutBuilder::FinishAddingShortcut(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackground,
                 url_,
                 title_,
                 shortcut_type_,
                 bitmap_result),
      true);
  Destroy();
}

bool ShortcutBuilder::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShortcutBuilder, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidRetrieveWebappInformation,
                        OnDidRetrieveWebappInformation)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ShortcutBuilder::WebContentsDestroyed() {
  Destroy();
}

void ShortcutBuilder::Destroy() {
  if (cancelable_task_tracker_.HasTrackedTasks()) {
    cancelable_task_tracker_.TryCancelAll();
  }
  delete this;
}

void ShortcutHelper::AddShortcut(content::WebContents* web_contents,
                                 const base::string16& title,
                                 int launcher_large_icon_size) {
  // The ShortcutBuilder deletes itself when it's done.
  new ShortcutBuilder(web_contents, title, launcher_large_icon_size);
}

bool ShortcutHelper::RegisterShortcutHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShortcutHelper::AddShortcutInBackground(
    const GURL& url,
    const base::string16& title,
    ShortcutBuilder::ShortcutType shortcut_type,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());

  // Grab the average color from the bitmap.
  SkColor color = SK_ColorWHITE;
  SkBitmap favicon_bitmap;
  if (bitmap_result.is_valid()) {
    if (gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                              bitmap_result.bitmap_data->size(),
                              &favicon_bitmap))
      color = color_utils::CalculateKMeanColorOfBitmap(favicon_bitmap);
  }

  int r_value = SkColorGetR(color);
  int g_value = SkColorGetG(color);
  int b_value = SkColorGetB(color);

  // Send the data to the Java side to create the shortcut.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> java_title =
      base::android::ConvertUTF16ToJavaString(env, title);
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (favicon_bitmap.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&favicon_bitmap);

  Java_ShortcutHelper_addShortcut(env,
                                  base::android::GetApplicationContext(),
                                  java_url.obj(),
                                  java_title.obj(),
                                  java_bitmap.obj(),
                                  r_value,
                                  g_value,
                                  b_value,
                                  shortcut_type != ShortcutBuilder::BOOKMARK);

  // Record what type of shortcut was added by the user.
  switch (shortcut_type) {
    case ShortcutBuilder::APP_SHORTCUT:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcut"));
      break;
    case ShortcutBuilder::APP_SHORTCUT_APPLE:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcutApple"));
      break;
    case ShortcutBuilder::BOOKMARK:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.Bookmark"));
      break;
    default:
      NOTREACHED();
  }
}

// Adds a shortcut to the current URL to the Android home screen, firing
// background tasks to pull all the data required.
// Note that we don't actually care about the tab here -- we just want
// its otherwise inaccessible WebContents.
static void AddShortcut(JNIEnv* env,
                        jclass clazz,
                        jlong tab_android_ptr,
                        jstring title,
                        jint launcher_large_icon_size) {
  TabAndroid* tab = reinterpret_cast<TabAndroid*>(tab_android_ptr);
  ShortcutHelper::AddShortcut(
      tab->web_contents(),
      base::android::ConvertJavaStringToUTF16(env, title),
      launcher_large_icon_size);
}
