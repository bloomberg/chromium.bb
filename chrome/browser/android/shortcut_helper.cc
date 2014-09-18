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
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/manifest.h"
#include "jni/ShortcutHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

jlong Initialize(JNIEnv* env, jobject obj, jlong tab_android_ptr) {
  TabAndroid* tab = reinterpret_cast<TabAndroid*>(tab_android_ptr);

  ShortcutHelper* shortcut_helper =
      new ShortcutHelper(env, obj, tab->web_contents());
  shortcut_helper->Initialize();

  return reinterpret_cast<intptr_t>(shortcut_helper);
}

ShortcutHelper::ShortcutHelper(JNIEnv* env,
                               jobject obj,
                               content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      java_ref_(env, obj),
      url_(web_contents->GetURL()),
      display_(content::Manifest::DISPLAY_MODE_BROWSER),
      weak_ptr_factory_(this) {
}

void ShortcutHelper::Initialize() {
  // Send a message to the renderer to retrieve information about the page.
  Send(new ChromeViewMsg_GetWebApplicationInfo(routing_id()));
}

ShortcutHelper::~ShortcutHelper() {
}

void ShortcutHelper::OnDidGetWebApplicationInfo(
    const WebApplicationInfo& received_web_app_info) {
  // Sanitize received_web_app_info.
  WebApplicationInfo web_app_info = received_web_app_info;
  web_app_info.title =
      web_app_info.title.substr(0, chrome::kMaxMetaTagAttributeLength);
  web_app_info.description =
      web_app_info.description.substr(0, chrome::kMaxMetaTagAttributeLength);

  title_ = web_app_info.title.empty() ? web_contents()->GetTitle()
                                      : web_app_info.title;

  if (web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE ||
      web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE_APPLE) {
    display_ = content::Manifest::DISPLAY_MODE_STANDALONE;
  }

  // Record what type of shortcut was added by the user.
  switch (web_app_info.mobile_capable) {
    case WebApplicationInfo::MOBILE_CAPABLE:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcut"));
      break;
    case WebApplicationInfo::MOBILE_CAPABLE_APPLE:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcutApple"));
      break;
    case WebApplicationInfo::MOBILE_CAPABLE_UNSPECIFIED:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.Bookmark"));
      break;
  }

  web_contents()->GetManifest(base::Bind(&ShortcutHelper::OnDidGetManifest,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void ShortcutHelper::OnDidGetManifest(const content::Manifest& manifest) {
  // Set the title based on the manifest value, if any.
  if (!manifest.short_name.is_null())
    title_ = manifest.short_name.string();
  else if (!manifest.name.is_null())
    title_ = manifest.name.string();

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    url_ = manifest.start_url;

  // Set the display based on the manifest value, if any.
  if (manifest.display != content::Manifest::DISPLAY_MODE_UNSPECIFIED)
    display_ = manifest.display;

  // 'fullscreen' and 'minimal-ui' are not yet supported, fallback to the right
  // mode in those cases.
  if (manifest.display == content::Manifest::DISPLAY_MODE_FULLSCREEN)
    display_ = content::Manifest::DISPLAY_MODE_STANDALONE;
  if (manifest.display == content::Manifest::DISPLAY_MODE_MINIMAL_UI)
    display_ = content::Manifest::DISPLAY_MODE_BROWSER;

  // The ShortcutHelper is now able to notify its Java counterpart that it is
  // initialized. OnInitialized method is not conceptually part of getting the
  // manifest data but it happens that the initialization is finalized when
  // these data are available.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  ScopedJavaLocalRef<jstring> j_title =
      base::android::ConvertUTF16ToJavaString(env, title_);

  Java_ShortcutHelper_onInitialized(env, j_obj.obj(), j_title.obj());
}

void ShortcutHelper::TearDown(JNIEnv*, jobject) {
  Destroy();
}

void ShortcutHelper::Destroy() {
  delete this;
}

void ShortcutHelper::AddShortcut(
    JNIEnv* env,
    jobject obj,
    jstring jtitle,
    jint launcher_large_icon_size) {
  base::string16 title = base::android::ConvertJavaStringToUTF16(env, jtitle);
  if (!title.empty())
    title_ = title;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

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
      base::Bind(&ShortcutHelper::FinishAddingShortcut,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

void ShortcutHelper::FinishAddingShortcut(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  icon_ = bitmap_result;

  // Stop observing so we don't get destroyed while doing the last steps.
  Observe(NULL);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackground,
                 url_,
                 title_,
                 display_,
                 icon_),
      true);

  Destroy();
}

bool ShortcutHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ShortcutHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidGetWebApplicationInfo,
                        OnDidGetWebApplicationInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ShortcutHelper::WebContentsDestroyed() {
  Destroy();
}

bool ShortcutHelper::RegisterShortcutHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShortcutHelper::AddShortcutInBackground(
    const GURL& url,
    const base::string16& title,
    content::Manifest::DisplayMode display,
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

  Java_ShortcutHelper_addShortcut(
      env,
      base::android::GetApplicationContext(),
      java_url.obj(),
      java_title.obj(),
      java_bitmap.obj(),
      r_value,
      g_value,
      b_value,
      display == content::Manifest::DISPLAY_MODE_STANDALONE);
}
