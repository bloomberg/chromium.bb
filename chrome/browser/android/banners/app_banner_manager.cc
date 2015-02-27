// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "chrome/browser/android/manifest_icon_selector.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/metrics/rappor/sampling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
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
base::TimeDelta gTimeDeltaForTesting;
bool gDisableSecureCheckForTesting = false;
const int kIconMinimumSize = 144;

// The requirement for now is an image/png that is at least 144x144.
bool DoesManifestContainRequiredIcon(const content::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    if (!EqualsASCII(icon.type.string(), "image/png"))
      continue;

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty()) // "any"
        return true;
      if (size.width() >= kIconMinimumSize && size.height() >= kIconMinimumSize)
        return true;
    }
  }

  return false;
}

}  // anonymous namespace

namespace banners {

// Fetches a bitmap and deletes itself when completed.
class AppBannerManager::BannerBitmapFetcher
    : public chrome::BitmapFetcher,
      public chrome::BitmapFetcherDelegate {
 public:
  BannerBitmapFetcher(const GURL& image_url,
                      banners::AppBannerManager* manager);

  // Prevents informing the AppBannerManager that the fetch has completed.
  void Cancel();

  // chrome::BitmapFetcherDelegate overrides
  void OnFetchComplete(const GURL url, const SkBitmap* icon) override;

 private:
  banners::AppBannerManager* manager_;
  bool is_cancelled_;
};

AppBannerManager::BannerBitmapFetcher::BannerBitmapFetcher(
    const GURL& image_url,
    banners::AppBannerManager* manager)
    : chrome::BitmapFetcher(image_url, this),
      manager_(manager),
      is_cancelled_(false) {
}

void AppBannerManager::BannerBitmapFetcher::Cancel() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  is_cancelled_ = true;
}

void AppBannerManager::BannerBitmapFetcher::OnFetchComplete(
    const GURL url,
    const SkBitmap* icon) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!is_cancelled_)
    manager_->OnFetchComplete(this, url, icon);

  delete this;
}

AppBannerManager::AppBannerManager(JNIEnv* env, jobject obj, int icon_size)
    : preferred_icon_size_(icon_size),
      fetcher_(nullptr),
      weak_java_banner_view_manager_(env, obj),
      weak_factory_(this) {
}

AppBannerManager::~AppBannerManager() {
  CancelActiveFetcher();
}

void AppBannerManager::Destroy(JNIEnv* env, jobject obj) {
  delete this;
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
  CancelActiveFetcher();
  app_title_ = base::string16();
  web_app_data_ = content::Manifest();
  native_app_data_.Reset();
  native_app_package_ = std::string();
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;
  validated_url_ = validated_url;

  // A secure scheme is required to show banners, so exit early if we see the
  // URL is invalid.
  if (!validated_url_.SchemeIsSecure() && !gDisableSecureCheckForTesting)
    return;

  // See if the page has a manifest.
  web_contents()->GetManifest(base::Bind(&AppBannerManager::OnDidGetManifest,
                                         weak_factory_.GetWeakPtr()));
}

// static
bool AppBannerManager::IsManifestValid(const content::Manifest& manifest) {
  if (manifest.IsEmpty())
    return false;
  if (!manifest.start_url.is_valid())
    return false;
  if (manifest.name.is_null() && manifest.short_name.is_null())
    return false;
  if (!DoesManifestContainRequiredIcon(manifest))
    return false;

  return true;
}

void AppBannerManager::OnDidGetManifest(const content::Manifest& manifest) {
  if (web_contents()->IsBeingDestroyed())
    return;

  if (!IsManifestValid(manifest)) {
    // No usable manifest, see if there is a play store meta tag.
    if (!IsEnabledForNativeApps())
      return;

    Send(new ChromeViewMsg_RetrieveMetaTagContent(routing_id(),
                                                  validated_url_,
                                                  kBannerTag));
    return;
  }

  banners::TrackDisplayEvent(DISPLAY_EVENT_BANNER_REQUESTED);

  web_app_data_ = manifest;
  app_title_ = web_app_data_.name.string();

  // Check to see if there is a single service worker controlling this page
  // and the manifest's start url.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartition(
          profile, web_contents()->GetSiteInstance());
  DCHECK(storage_partition);

  storage_partition->GetServiceWorkerContext()->CheckHasServiceWorker(
      validated_url_, manifest.start_url,
      base::Bind(&AppBannerManager::OnDidCheckHasServiceWorker,
                 weak_factory_.GetWeakPtr()));
}

void AppBannerManager::OnDidCheckHasServiceWorker(bool has_service_worker) {
  if (has_service_worker) {
    // TODO(benwells): Check triggering parameters.
    // Create an infobar to promote the manifest's app.
    GURL icon_url =
        ManifestIconSelector::FindBestMatchingIcon(
            web_app_data_.icons,
            preferred_icon_size_,
            gfx::Screen::GetScreenFor(web_contents()->GetNativeView()));
    if (icon_url.is_empty())
      return;

    FetchIcon(icon_url);
  } else {
    TrackDisplayEvent(DISPLAY_EVENT_LACKS_SERVICE_WORKER);
  }
}

void AppBannerManager::RecordCouldShowBanner(
    const std::string& package_or_start_url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), validated_url_, package_or_start_url,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, GetCurrentTime());
}

bool AppBannerManager::CheckIfShouldShow(
    const std::string& package_or_start_url) {
  if (!AppBannerSettingsHelper::ShouldShowBanner(web_contents(), validated_url_,
                                                 package_or_start_url,
                                                 GetCurrentTime())) {
    return false;
  }

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), validated_url_, package_or_start_url,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, GetCurrentTime());
  return true;
}

void AppBannerManager::CancelActiveFetcher() {
  // Fetchers delete themselves.
  if (fetcher_ != nullptr) {
    fetcher_->Cancel();
    fetcher_ = nullptr;
  }
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

void AppBannerManager::OnFetchComplete(BannerBitmapFetcher* fetcher,
                                       const GURL url,
                                       const SkBitmap* bitmap) {
  if (fetcher_ != fetcher)
    return;
  fetcher_ = nullptr;

  if (!web_contents()
      || web_contents()->IsBeingDestroyed()
      || validated_url_ != web_contents()->GetURL()
      || !bitmap
      || url != app_icon_url_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;

  InfoBarService* service = InfoBarService::FromWebContents(web_contents());

  AppBannerInfoBar* weak_infobar_ptr = nullptr;
  if (!native_app_data_.is_null()) {
    RecordCouldShowBanner(native_app_package_);
    if (!CheckIfShouldShow(native_app_package_))
      return;

    weak_infobar_ptr = AppBannerInfoBarDelegate::CreateForNativeApp(
        service,
        app_title_,
        new SkBitmap(*bitmap),
        native_app_data_,
        native_app_package_);

    rappor::SampleDomainAndRegistryFromGURL("AppBanner.NativeApp.Shown",
                                            web_contents()->GetURL());
  } else if (!web_app_data_.IsEmpty()){
    RecordCouldShowBanner(web_app_data_.start_url.spec());
    if (!CheckIfShouldShow(web_app_data_.start_url.spec()))
      return;

    weak_infobar_ptr = AppBannerInfoBarDelegate::CreateForWebApp(
        service,
        app_title_,
        new SkBitmap(*bitmap),
        web_app_data_);

    rappor::SampleDomainAndRegistryFromGURL("AppBanner.WebApp.Shown",
                                            web_contents()->GetURL());
  }

  if (weak_infobar_ptr != nullptr)
    banners::TrackDisplayEvent(DISPLAY_EVENT_CREATED);
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

  banners::TrackDisplayEvent(DISPLAY_EVENT_BANNER_REQUESTED);

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, expected_url.spec()));
  ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, tag_content));
  Java_AppBannerManager_fetchAppDetails(env,
                                        jobj.obj(),
                                        jurl.obj(),
                                        jpackage.obj(),
                                        preferred_icon_size_);
}

bool AppBannerManager::OnAppDetailsRetrieved(JNIEnv* env,
                                             jobject obj,
                                             jobject japp_data,
                                             jstring japp_title,
                                             jstring japp_package,
                                             jstring jicon_url) {
  if (validated_url_ != web_contents()->GetURL())
    return false;

  std::string image_url = ConvertJavaStringToUTF8(env, jicon_url);
  app_title_ = ConvertJavaStringToUTF16(env, japp_title);
  native_app_package_ = ConvertJavaStringToUTF8(env, japp_package);
  native_app_data_.Reset(env, japp_data);
  return FetchIcon(GURL(image_url));
}

bool AppBannerManager::IsFetcherActive(JNIEnv* env, jobject obj) {
  return fetcher_ != nullptr;
}

bool AppBannerManager::FetchIcon(const GURL& image_url) {
  if (!web_contents())
    return false;

  // Begin asynchronously fetching the app icon.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  CancelActiveFetcher();
  fetcher_ = new BannerBitmapFetcher(image_url, this);
  fetcher_->Start(
      profile->GetRequestContext(),
      std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::LOAD_NORMAL);
  app_icon_url_ = image_url;
  return true;
}

// static
bool AppBannerManager::IsEnabledForNativeApps() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAppInstallAlerts);
}

// static
base::Time AppBannerManager::GetCurrentTime() {
  return base::Time::Now() + gTimeDeltaForTesting;
}

jlong Init(JNIEnv* env, jobject obj, jint icon_size) {
  AppBannerManager* manager = new AppBannerManager(env, obj, icon_size);
  return reinterpret_cast<intptr_t>(manager);
}

jboolean IsEnabled(JNIEnv* env, jclass clazz) {
  const std::string group_name =
      base::FieldTrialList::FindFullName("AppBanners");
  return group_name == "Enabled";
}

void SetTimeDeltaForTesting(JNIEnv* env, jclass clazz, jint days) {
  gTimeDeltaForTesting = base::TimeDelta::FromDays(days);
}

void DisableSecureSchemeCheckForTesting(JNIEnv* env, jclass clazz) {
  gDisableSecureCheckForTesting = true;
}

// Register native methods
bool RegisterAppBannerManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace banners
