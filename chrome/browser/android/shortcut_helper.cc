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
#include "net/base/mime_util.h"
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
      orientation_(blink::WebScreenOrientationLockDefault),
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

bool ShortcutHelper::IconSizesContainsPreferredSize(
    const std::vector<gfx::Size>& sizes) const {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].height() != sizes[i].width())
      continue;
    if (sizes[i].width() == preferred_icon_size_in_px_)
      return true;
  }

  return false;
}

bool ShortcutHelper::IconSizesContainsAny(
    const std::vector<gfx::Size>& sizes) const {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].IsEmpty())
      return true;
  }

  return false;
}

GURL ShortcutHelper::FindBestMatchingIcon(
    const std::vector<Manifest::Icon>& icons, float density) const {
  GURL url;
  int best_delta = std::numeric_limits<int>::min();

  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density != density)
      continue;

    const std::vector<gfx::Size>& sizes = icons[i].sizes;
    for (size_t j = 0; j < sizes.size(); ++j) {
      if (sizes[j].height() != sizes[j].width())
        continue;
      int delta = sizes[j].width() - preferred_icon_size_in_px_;
      if (delta == 0)
        return icons[i].src;
      if (best_delta > 0 && delta < 0)
        continue;
      if ((best_delta > 0 && delta < best_delta) ||
          (best_delta < 0 && delta > best_delta)) {
        url = icons[i].src;
        best_delta = delta;
      }
    }
  }

  return url;
}

// static
std::vector<Manifest::Icon> ShortcutHelper::FilterIconsByType(
    const std::vector<Manifest::Icon>& icons) {
  std::vector<Manifest::Icon> result;

  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].type.is_null() ||
        net::IsSupportedImageMimeType(
            base::UTF16ToUTF8(icons[i].type.string()))) {
      result.push_back(icons[i]);
    }
  }

  return result;
}

GURL ShortcutHelper::FindBestMatchingIcon(
    const std::vector<Manifest::Icon>& unfiltered_icons) const {
  const float device_scale_factor =
      gfx::Screen::GetScreenFor(web_contents()->GetNativeView())->
          GetPrimaryDisplay().device_scale_factor();

  GURL url;
  std::vector<Manifest::Icon> icons = FilterIconsByType(unfiltered_icons);

  // The first pass is to find the ideal icon. That icon is of the right size
  // with the default density or the device's density.
  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density == device_scale_factor &&
        IconSizesContainsPreferredSize(icons[i].sizes)) {
      return icons[i].src;
    }

    // If there is an icon with the right size but not the right density, keep
    // it on the side and only use it if nothing better is found.
    if (icons[i].density == Manifest::Icon::kDefaultDensity &&
        IconSizesContainsPreferredSize(icons[i].sizes)) {
      url = icons[i].src;
    }
  }

  // The second pass is to find an icon with 'any'. The current device scale
  // factor is preferred. Otherwise, the default scale factor is used.
  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density == device_scale_factor &&
        IconSizesContainsAny(icons[i].sizes)) {
      return icons[i].src;
    }

    // If there is an icon with 'any' but not the right density, keep it on the
    // side and only use it if nothing better is found.
    if (icons[i].density == Manifest::Icon::kDefaultDensity &&
        IconSizesContainsAny(icons[i].sizes)) {
      url = icons[i].src;
    }
  }

  // The last pass will try to find the best suitable icon for the device's
  // scale factor. If none, another pass will be run using kDefaultDensity.
  if (!url.is_valid())
    url = FindBestMatchingIcon(icons, device_scale_factor);
  if (!url.is_valid())
    url = FindBestMatchingIcon(icons, Manifest::Icon::kDefaultDensity);

  return url;
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

  // Set the orientation based on the manifest value, if any.
  if (manifest.orientation != blink::WebScreenOrientationLockDefault) {
    // Ignore the orientation if the display mode is different from
    // 'standalone'.
    // TODO(mlamouri): send a message to the developer console about this.
    if (display_ == content::Manifest::DISPLAY_MODE_STANDALONE)
      orientation_ = manifest.orientation;
  }

  GURL icon_src = FindBestMatchingIcon(manifest.icons);
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
      base::android::ConvertUTF16ToJavaString(env, title_);

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
    title_ = title;

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
  // Stop observing so we don't get destroyed while doing the last steps.
  Observe(NULL);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithSkBitmap,
                 url_,
                 title_,
                 display_,
                 manifest_icon_,
                 orientation_),
      true);

  Destroy();
}

void ShortcutHelper::AddShortcutUsingFavicon() {
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
  int threshold_to_get_any_largest_icon = preferred_icon_size_in_px_ - 1;
  favicon_service->GetLargestRawFaviconForPageURL(url_, icon_types,
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
                 url_,
                 title_,
                 display_,
                 bitmap_result,
                 orientation_),
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
    const GURL& url,
    const base::string16& title,
    content::Manifest::DisplayMode display,
    const favicon_base::FaviconRawBitmapResult& bitmap_result,
    blink::WebScreenOrientationLockType orientation) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());

  SkBitmap icon_bitmap;
  if (bitmap_result.is_valid()) {
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                          bitmap_result.bitmap_data->size(),
                          &icon_bitmap);
  }

  AddShortcutInBackgroundWithSkBitmap(
      url, title, display, icon_bitmap, orientation);
}

void ShortcutHelper::AddShortcutInBackgroundWithSkBitmap(
    const GURL& url,
    const base::string16& title,
    content::Manifest::DisplayMode display,
    const SkBitmap& icon_bitmap,
    blink::WebScreenOrientationLockType orientation) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());

  SkColor color = color_utils::CalculateKMeanColorOfBitmap(icon_bitmap);
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
      display == content::Manifest::DISPLAY_MODE_STANDALONE,
      orientation);
}
