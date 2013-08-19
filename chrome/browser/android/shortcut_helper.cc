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
#include "base/threading/worker_pool.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/ShortcutHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "url/gurl.h"

namespace {

// Adds a shortcut to the current URL to the Android home screen.
// This proceeds over three phases:
// 1) The renderer is asked to parse out webapp related meta tags with an async
//    IPC message.
// 2) The highest-resolution favicon available is retrieved for use as the
//    icon on the home screen.
// 3) A JNI call is made to fire an Intent at the Android launcher, which adds
//    adds the shortcut.
class ShortcutBuilder : public content::WebContentsObserver {
 public:
  explicit ShortcutBuilder(content::WebContents* web_contents);
  virtual ~ShortcutBuilder();

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents* contents) OVERRIDE;

 private:
  void OnDidRetrieveWebappInformation(bool success, bool is_webapp_capable);

  Profile* profile_;
  GURL url_;
  string16 title_;
  bool is_webapp_capable_;
  CancelableTaskTracker cancelable_task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutBuilder);
};

ShortcutBuilder::ShortcutBuilder(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  profile_ =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  url_ = web_contents->GetURL();
  title_ = web_contents->GetTitle();

  // Send a message to the renderer to retrieve information about the page.
  Send(new ChromeViewMsg_RetrieveWebappInformation(routing_id(), url_));
}

ShortcutBuilder::~ShortcutBuilder() {
}

void ShortcutBuilder::OnDidRetrieveWebappInformation(bool success,
                                                     bool is_webapp_capable) {
  // Abort adding the shortcut if the renderer failed to process the page or
  // if a navigation was instantiated before the process finished.
  if (!success) {
    LOG(ERROR) << "Failed to parse webpage.";
    return;
  }
  is_webapp_capable_ = is_webapp_capable;

  // Grab the best, largest icon we can find to represent this bookmark.
  // TODO(dfalcantara): Try combining with the new BookmarksHandler once its
  //                    rewrite is further along.
  FaviconService::FaviconForURLParams favicon_params(
      profile_,
      url_,
      chrome::TOUCH_PRECOMPOSED_ICON | chrome::TOUCH_ICON | chrome::FAVICON,
      0);

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);

  favicon_service->GetRawFaviconForURL(
      favicon_params,
      ui::SCALE_FACTOR_100P,
      base::Bind(&ShortcutHelper::FinishAddingShortcut,
                 url_,
                 title_,
                 is_webapp_capable_),
      &cancelable_task_tracker_);
}

void ShortcutBuilder::WebContentsDestroyed(content::WebContents* web_contents) {
  if (cancelable_task_tracker_.HasTrackedTasks())
    cancelable_task_tracker_.TryCancelAll();
  delete this;
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

}  // namespace

void ShortcutHelper::AddShortcut(content::WebContents* web_contents) {
  // The ShortcutBuilder deletes itself when it's done.
  new ShortcutBuilder(web_contents);
}

bool ShortcutHelper::RegisterShortcutHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShortcutHelper::FinishAddingShortcut(
    const GURL& url,
    const string16& title,
    bool is_webapp_capable,
    const chrome::FaviconBitmapResult& bitmap_result) {
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ShortcutHelper::FinishAddingShortcutInBackground,
                 url,
                 title,
                 is_webapp_capable,
                 bitmap_result),
      true);
}

void ShortcutHelper::FinishAddingShortcutInBackground(
    const GURL& url,
    const string16& title,
    bool is_webapp_capable,
    const chrome::FaviconBitmapResult& bitmap_result) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());

  // Grab the average color from the bitmap.
  SkColor color = SK_ColorWHITE;
  SkBitmap favicon_bitmap;
  if (bitmap_result.is_valid()) {
    color_utils::GridSampler sampler;
    color = color_utils::CalculateKMeanColorOfPNG(bitmap_result.bitmap_data,
                                                  100,
                                                  665,
                                                  &sampler);
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                          bitmap_result.bitmap_data->size(),
                          &favicon_bitmap);
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
                                  is_webapp_capable);
}

// Adds a shortcut to the current URL to the Android home screen, firing
// background tasks to pull all the data required.
// Note that we don't actually care about the tab here -- we just want
// its otherwise inaccessible WebContents.
static void AddShortcut(JNIEnv* env, jclass clazz, jint tab_android_ptr) {
  TabAndroid* tab = reinterpret_cast<TabAndroid*>(tab_android_ptr);
  ShortcutHelper::AddShortcut(tab->GetWebContents());
}
