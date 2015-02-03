// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "chrome/browser/android/banners/app_banner_metrics_ids.h"
#include "chrome/browser/android/banners/app_banner_utilities.h"
#include "chrome/browser/android/manifest_icon_selector.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/manifest.h"
#include "jni/AppBannerManager_jni.h"
#include "net/base/load_flags.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/screen.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace {
const char kBannerTag[] = "google-play-id";
}

namespace banners {

AppBannerManager::AppBannerManager(JNIEnv* env, jobject obj)
    : weak_java_banner_view_manager_(env, obj) {}

AppBannerManager::~AppBannerManager() {
}

void AppBannerManager::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void AppBannerManager::BlockBanner(JNIEnv* env,
                                   jobject obj,
                                   jstring jurl,
                                   jstring jpackage) {
  if (!web_contents())
    return;

  GURL url(ConvertJavaStringToUTF8(env, jurl));
  std::string package_name = ConvertJavaStringToUTF8(env, jpackage);
  AppBannerSettingsHelper::Block(web_contents(), url, package_name);
}

void AppBannerManager::Block() const {
  if (!web_contents() || manifest_.IsEmpty())
    return;

  AppBannerSettingsHelper::Block(web_contents(),
                                 web_contents()->GetURL(),
                                 manifest_.start_url.spec());
}

void AppBannerManager::Install() const {
  if (!web_contents())
    return;

  if (!manifest_.IsEmpty()) {
    // TODO(dfalcantara): Trigger shortcut creation.
  }
}

gfx::Image AppBannerManager::GetIcon() const {
  return gfx::Image::CreateFrom1xBitmap(*app_icon_.get());
}

void AppBannerManager::ReplaceWebContents(JNIEnv* env,
                                          jobject obj,
                                          jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  Observe(web_contents);
}

void AppBannerManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Clear current state.
  fetcher_.reset();
  manifest_ = content::Manifest();
  app_icon_.reset();

  // Get rid of the current banner.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;
  Java_AppBannerManager_dismissCurrentBanner(env, jobj.obj(), DISMISS_NAVIGATE);
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;
  validated_url_ = validated_url;

  // See if the page has a manifest. Using Unretained(this) here is safe as the
  // lifetime of this object extends beyond the lifetime of the web_contents(),
  // and when web_contents() is destroyed it will call OnDidGetManifest.
  web_contents()->GetManifest(base::Bind(&AppBannerManager::OnDidGetManifest,
                                         base::Unretained(this)));
}

void AppBannerManager::OnDidGetManifest(const content::Manifest& manifest) {
  if (web_contents()->IsBeingDestroyed())
    return;

  if (manifest.IsEmpty()) {
    // No manifest, see if there is a play store meta tag.
    Send(new ChromeViewMsg_RetrieveMetaTagContent(routing_id(),
                                                  validated_url_,
                                                  kBannerTag));
    return;
  }

  // TODO(benwells): Check triggering parameters here and if there is a meta
  // tag.

  // Create an infobar to promote the manifest's app.
  manifest_ = manifest;

  GURL icon_url =
      ManifestIconSelector::FindBestMatchingIcon(
          manifest.icons,
          GetPreferredIconSize(),
          gfx::Screen::GetScreenFor(web_contents()->GetNativeView()));
  if (icon_url.is_empty())
    return;

  FetchIcon(icon_url);
}

bool AppBannerManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppBannerManager, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidRetrieveMetaTagContent,
                        OnDidRetrieveMetaTagContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AppBannerManager::OnFetchComplete(const GURL url, const SkBitmap* bitmap) {
  if (bitmap) {
    JNIEnv* env = base::android::AttachCurrentThread();

    ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
    if (jobj.is_null())
      return;

    bool displayed;
    if (manifest_.IsEmpty()) {
      ScopedJavaLocalRef<jobject> jimage = gfx::ConvertToJavaBitmap(bitmap);
      ScopedJavaLocalRef<jstring> jimage_url(
          ConvertUTF8ToJavaString(env, url.spec()));

      displayed = Java_AppBannerManager_createBanner(env,
                                                     jobj.obj(),
                                                     jimage_url.obj(),
                                                     jimage.obj());
    } else {
      app_icon_.reset(new SkBitmap(*bitmap));
      InfoBarService* service = InfoBarService::FromWebContents(web_contents());
      displayed = AppBannerInfoBarDelegate::CreateForWebApp(
          service,
          this,
          manifest_.name.string(),
          manifest_.start_url) != NULL;
    }

    if (displayed)
      banners::TrackDisplayEvent(DISPLAY_CREATED);
  } else {
    DVLOG(1) << "Failed to retrieve image: " << url;
  }

  fetcher_.reset();
}

void AppBannerManager::OnDidRetrieveMetaTagContent(
    bool success,
    const std::string& tag_name,
    const std::string& tag_content,
    const GURL& expected_url) {
  DCHECK(web_contents());
  if (!success || tag_name != kBannerTag || validated_url_ != expected_url ||
      tag_content.size() >= chrome::kMaxMetaTagAttributeLength) {
    return;
  }

  banners::TrackDisplayEvent(DISPLAY_BANNER_REQUESTED);

  if (!AppBannerSettingsHelper::IsAllowed(web_contents(),
                                          expected_url,
                                          tag_content)) {
    return;
  }

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, expected_url.spec()));
  ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, tag_content));
  Java_AppBannerManager_prepareBanner(env,
                                      jobj.obj(),
                                      jurl.obj(),
                                      jpackage.obj());
}

bool AppBannerManager::FetchIcon(JNIEnv* env,
                                 jobject obj,
                                 jstring jimage_url) {
  std::string image_url = ConvertJavaStringToUTF8(env, jimage_url);
  return FetchIcon(GURL(image_url));
}

bool AppBannerManager::FetchIcon(const GURL& image_url) {
  if (!web_contents())
    return false;

  // Begin asynchronously fetching the app icon.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  fetcher_.reset(new chrome::BitmapFetcher(image_url, this));
  fetcher_.get()->Start(
      profile->GetRequestContext(),
      std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::LOAD_NORMAL);
  return true;
}

int AppBannerManager::GetPreferredIconSize() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return 0;

  return Java_AppBannerManager_getPreferredIconSize(env, jobj.obj());
}

void RecordDismissEvent(JNIEnv* env, jclass clazz, jint metric) {
  banners::TrackDismissEvent(metric);
}

void RecordInstallEvent(JNIEnv* env, jclass clazz, jint metric) {
  banners::TrackInstallEvent(metric);
}

jlong Init(JNIEnv* env, jobject obj) {
  AppBannerManager* manager = new AppBannerManager(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}

jboolean IsEnabled(JNIEnv* env, jclass clazz) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAppInstallAlerts);
}

// Register native methods
bool RegisterAppBannerManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace banners
