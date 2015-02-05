// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "chrome/browser/android/banners/app_banner_metrics_ids.h"
#include "chrome/browser/android/banners/app_banner_utilities.h"
#include "chrome/browser/android/manifest_icon_selector.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
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
using base::android::ConvertJavaStringToUTF16;
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
  if (!web_contents())
    return;

  if (!web_app_data_.IsEmpty()) {
    AppBannerSettingsHelper::Block(web_contents(),
                                   web_contents()->GetURL(),
                                   web_app_data_.start_url.spec());
  }
}

void AppBannerManager::OnInfoBarDestroyed() {
  weak_infobar_ptr_ = nullptr;
}

bool AppBannerManager::OnButtonClicked() const {
  if (!web_contents())
    return true;

  if (!web_app_data_.IsEmpty()) {
    InstallManifestApp(web_app_data_, *app_icon_.get());
    return true;
  }

  return true;
}

base::string16 AppBannerManager::GetTitle() const {
  return app_title_;
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
  app_title_ = base::string16();
  app_icon_.reset();
  web_app_data_ = content::Manifest();
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

  if (manifest.IsEmpty()
      || !manifest.start_url.is_valid()
      || (manifest.name.is_null() && manifest.short_name.is_null())) {
    // No usable manifest, see if there is a play store meta tag.
    Send(new ChromeViewMsg_RetrieveMetaTagContent(routing_id(),
                                                  validated_url_,
                                                  kBannerTag));
    return;
  }

  // TODO(benwells): Check triggering parameters here and if there is a meta
  // tag.

  // Create an infobar to promote the manifest's app.
  web_app_data_ = manifest;
  app_title_ = web_app_data_.name.string();
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
  fetcher_.reset();
  if (!bitmap || url != app_icon_url_) {
    DVLOG(1) << "Failed to retrieve image: " << url;
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;

  app_icon_.reset(new SkBitmap(*bitmap));
  InfoBarService* service = InfoBarService::FromWebContents(web_contents());

  weak_infobar_ptr_ = nullptr;
  if (!web_app_data_.IsEmpty()){
    weak_infobar_ptr_ = AppBannerInfoBarDelegate::CreateForWebApp(
        service,
        this,
        web_app_data_.start_url);
  }

  if (weak_infobar_ptr_ != nullptr)
    banners::TrackDisplayEvent(DISPLAY_CREATED);
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
  app_icon_url_ = image_url;
  return true;
}

int AppBannerManager::GetPreferredIconSize() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return 0;

  return Java_AppBannerManager_getPreferredIconSize(env, jobj.obj());
}

// static
void AppBannerManager::InstallManifestApp(const content::Manifest& manifest,
                                          const SkBitmap& icon) {
  ShortcutInfo info;
  info.UpdateFromManifest(manifest);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithSkBitmap,
                 info,
                 icon),
      true);
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
