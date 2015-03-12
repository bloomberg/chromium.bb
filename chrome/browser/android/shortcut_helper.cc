// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>
#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/manifest/manifest_icon_selector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/web_application_info.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/manifest.h"
#include "jni/ShortcutHelper_jni.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/screen.h"
#include "url/gurl.h"

using content::Manifest;

// Android's preferred icon size in DP is 48, as defined in
// http://developer.android.com/design/style/iconography.html
const int ShortcutHelper::kPreferredIconSizeInDp = 48;

jlong Initialize(JNIEnv* env, jobject obj, jobject java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  ShortcutHelper* shortcut_helper = new ShortcutHelper(env, obj, web_contents);
  shortcut_helper->Initialize();

  return reinterpret_cast<intptr_t>(shortcut_helper);
}

ShortcutHelper::ShortcutHelper(JNIEnv* env,
                               jobject obj,
                               content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      java_ref_(env, obj),
      shortcut_info_(dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
                     web_contents->GetURL())),
      add_shortcut_requested_(false),
      manifest_icon_status_(MANIFEST_ICON_STATUS_NONE),
      preferred_icon_size_in_px_(kPreferredIconSizeInDp *
          gfx::Screen::GetScreenFor(web_contents->GetNativeView())->
              GetPrimaryDisplay().device_scale_factor()),
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

  shortcut_info_.title = web_app_info.title.empty() ? web_contents()->GetTitle()
                                                    : web_app_info.title;

  if (web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE ||
      web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE_APPLE) {
    shortcut_info_.display = content::Manifest::DISPLAY_MODE_STANDALONE;
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
  if (!manifest.IsEmpty()) {
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.Manifest"));
  }

  shortcut_info_.UpdateFromManifest(manifest);

  GURL icon_src = ManifestIconSelector::FindBestMatchingIcon(
      manifest.icons,
      kPreferredIconSizeInDp,
      gfx::Screen::GetScreenFor(web_contents()->GetNativeView()));
  if (icon_src.is_valid()) {
    web_contents()->DownloadImage(icon_src,
                                  false,
                                  preferred_icon_size_in_px_,
                                  base::Bind(&ShortcutHelper::OnDidDownloadIcon,
                                             weak_ptr_factory_.GetWeakPtr()));
    manifest_icon_status_ = MANIFEST_ICON_STATUS_FETCHING;
  }

  // The ShortcutHelper is now able to notify its Java counterpart that it is
  // initialized. OnInitialized method is not conceptually part of getting the
  // manifest data but it happens that the initialization is finalized when
  // these data are available.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  ScopedJavaLocalRef<jstring> j_title =
      base::android::ConvertUTF16ToJavaString(env, shortcut_info_.title);

  Java_ShortcutHelper_onInitialized(env, j_obj.obj(), j_title.obj());
}

void ShortcutHelper::OnDidDownloadIcon(int id,
                                       int http_status_code,
                                       const GURL& url,
                                       const std::vector<SkBitmap>& bitmaps,
                                       const std::vector<gfx::Size>& sizes) {
  // If getting the candidate manifest icon failed, the ShortcutHelper should
  // fallback to the favicon.
  // If the user already requested to add the shortcut, it will do so but use
  // the favicon instead.
  // Otherwise, it sets the state as if there was no manifest icon pending.
  if (bitmaps.empty()) {
    if (add_shortcut_requested_)
      AddShortcutUsingFavicon();
    else
      manifest_icon_status_ = MANIFEST_ICON_STATUS_NONE;
    return;
  }

  // There might be multiple bitmaps returned. The one to pick is bigger or
  // equal to the preferred size. |bitmaps| is ordered from bigger to smaller.
  int preferred_bitmap_index = 0;
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() < preferred_icon_size_in_px_)
      break;
    preferred_bitmap_index = i;
  }

  manifest_icon_ = bitmaps[preferred_bitmap_index];
  manifest_icon_status_ = MANIFEST_ICON_STATUS_DONE;

  if (add_shortcut_requested_)
    AddShortcutUsingManifestIcon();
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
  add_shortcut_requested_ = true;

  base::string16 title = base::android::ConvertJavaStringToUTF16(env, jtitle);
  if (!title.empty())
    shortcut_info_.title = title;

  switch (manifest_icon_status_) {
    case MANIFEST_ICON_STATUS_NONE:
      AddShortcutUsingFavicon();
      break;
    case MANIFEST_ICON_STATUS_FETCHING:
      // ::OnDidDownloadIcon() will call AddShortcutUsingManifestIcon().
      break;
    case MANIFEST_ICON_STATUS_DONE:
      AddShortcutUsingManifestIcon();
      break;
  }
}

void ShortcutHelper::AddShortcutUsingManifestIcon() {
  RecordAddToHomescreen();

  // Stop observing so we don't get destroyed while doing the last steps.
  Observe(NULL);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithSkBitmap,
                 shortcut_info_,
                 manifest_icon_,
                 true),
      true);

  Destroy();
}

void ShortcutHelper::AddShortcutUsingFavicon() {
  RecordAddToHomescreen();

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
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  // Using favicon if its size is not smaller than platform required size,
  // otherwise using the largest icon among all avaliable icons.
  int threshold_to_get_any_largest_icon = preferred_icon_size_in_px_ - 1;
  favicon_service->GetLargestRawFaviconForPageURL(
      shortcut_info_.url,
      icon_types,
      threshold_to_get_any_largest_icon,
      base::Bind(&ShortcutHelper::OnDidGetFavicon,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

void ShortcutHelper::OnDidGetFavicon(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  // Stop observing so we don't get destroyed while doing the last steps.
  Observe(NULL);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithRawBitmap,
                 shortcut_info_,
                 bitmap_result),
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

void ShortcutHelper::AddShortcutInBackgroundWithRawBitmap(
    const ShortcutInfo& info,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());

  SkBitmap icon_bitmap;
  if (bitmap_result.is_valid()) {
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                          bitmap_result.bitmap_data->size(),
                          &icon_bitmap);
  }

  AddShortcutInBackgroundWithSkBitmap(info, icon_bitmap, true);
}

void ShortcutHelper::AddShortcutInBackgroundWithSkBitmap(
    const ShortcutInfo& info,
    const SkBitmap& icon_bitmap,
    const bool return_to_homescreen) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());

  SkColor color = color_utils::CalculateKMeanColorOfBitmap(icon_bitmap);
  int r_value = SkColorGetR(color);
  int g_value = SkColorGetG(color);
  int b_value = SkColorGetB(color);

  // Send the data to the Java side to create the shortcut.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_title =
      base::android::ConvertUTF16ToJavaString(env, info.title);
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (icon_bitmap.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon_bitmap);

  Java_ShortcutHelper_addShortcut(
      env,
      base::android::GetApplicationContext(),
      java_url.obj(),
      java_title.obj(),
      java_bitmap.obj(),
      r_value,
      g_value,
      b_value,
      info.display == content::Manifest::DISPLAY_MODE_STANDALONE,
      info.orientation,
      return_to_homescreen);
}

void ShortcutHelper::RecordAddToHomescreen() {
  // Record that the shortcut has been added, so no banners will be shown
  // for this app.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), shortcut_info_.url, shortcut_info_.url.spec(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}
