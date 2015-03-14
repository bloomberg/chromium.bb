// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_data_fetcher.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/manifest/manifest_icon_selector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "components/infobars/core/infobar.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"
#include "ui/gfx/screen.h"

namespace {

base::TimeDelta gTimeDeltaForTesting;
int gCurrentRequestID = -1;

// The requirement for now is an image/png that is at least 144x144.
const int kIconMinimumSize = 144;
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

// static
base::Time AppBannerDataFetcher::GetCurrentTime() {
  return base::Time::Now() + gTimeDeltaForTesting;
}

// static
void AppBannerDataFetcher::SetTimeDeltaForTesting(int days) {
  gTimeDeltaForTesting = base::TimeDelta::FromDays(days);
}

AppBannerDataFetcher::AppBannerDataFetcher(
    content::WebContents* web_contents,
    base::WeakPtr<Delegate> delegate,
    int ideal_icon_size)
    : WebContentsObserver(web_contents),
      ideal_icon_size_(ideal_icon_size),
      weak_delegate_(delegate),
      is_active_(false) {
}

void AppBannerDataFetcher::Start(const GURL& validated_url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  is_active_ = true;
  validated_url_ = validated_url;
  web_contents->GetManifest(
      base::Bind(&AppBannerDataFetcher::OnDidGetManifest, this));
}

void AppBannerDataFetcher::Cancel() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (is_active_) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnDecidedWhetherToShow(this, false));
    is_active_ = false;
  }
}

void AppBannerDataFetcher::ReplaceWebContents(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

void AppBannerDataFetcher::AddObserverForTesting(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AppBannerDataFetcher::RemoveObserverForTesting(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppBannerDataFetcher::WebContentsDestroyed() {
  Cancel();
  Observe(nullptr);
}

void AppBannerDataFetcher::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  Cancel();
}

bool AppBannerDataFetcher::OnMessageReceived(
    const IPC::Message& message, content::RenderFrameHost* render_frame_host) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(AppBannerDataFetcher, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AppBannerPromptReply,
                        OnBannerPromptReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AppBannerDataFetcher::OnBannerPromptReply(
    content::RenderFrameHost* render_frame_host,
    int request_id,
    blink::WebAppBannerPromptReply reply) {
  content::WebContents* web_contents = GetWebContents();
  if (!is_active_ || !web_contents || request_id != gCurrentRequestID) {
    Cancel();
    return;
  }
  is_active_ = false;

  // The renderer might have requested the prompt to be canceled.
  if (reply == blink::WebAppBannerPromptReply::Cancel) {
    // TODO(mlamouri,benwells): we should probably record that to behave
    // differently with regard to showing the banner.
    Cancel();
    return;
  }

  // Definitely going to show the banner now.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnDecidedWhetherToShow(this, true));

  infobars::InfoBar* infobar = CreateBanner(app_icon_.get(), app_title_);
  if (infobar) {
    InfoBarService::FromWebContents(web_contents)->AddInfoBar(
        make_scoped_ptr(infobar));
  }
}

AppBannerDataFetcher::~AppBannerDataFetcher() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnFetcherDestroyed(this));
}

std::string AppBannerDataFetcher::GetBannerType() {
  return "web";
}

content::WebContents* AppBannerDataFetcher::GetWebContents() {
  if (!web_contents() || web_contents()->IsBeingDestroyed())
    return nullptr;
  return web_contents();
}

std::string AppBannerDataFetcher::GetAppIdentifier() {
  DCHECK(!web_app_data_.IsEmpty());
  return web_app_data_.start_url.spec();
}

bool AppBannerDataFetcher::FetchIcon(const GURL& image_url) {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  // Begin asynchronously fetching the app icon.  AddRef() is done before the
  // fetch to ensure that the AppBannerDataFetcher isn't deleted before the
  // BitmapFetcher has called OnFetchComplete() (where the references are
  // decremented).
  AddRef();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bitmap_fetcher_.reset(new chrome::BitmapFetcher(image_url, this));
  bitmap_fetcher_->Start(
      profile->GetRequestContext(),
      std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::LOAD_NORMAL);
  return true;
}

infobars::InfoBar* AppBannerDataFetcher::CreateBanner(
    const SkBitmap* icon,
    const base::string16& title) {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents && !web_app_data_.IsEmpty());

  // TODO(dfalcantara): Desktop doesn't display app banners, yet.  Just pretend
  //                    that a banner was shown for testing purposes.
  RecordDidShowBanner("AppBanner.WebApp.Shown");
  return nullptr;
}

void AppBannerDataFetcher::RecordDidShowBanner(const std::string& event_name) {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW,
      GetCurrentTime());
  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          event_name,
                                          web_contents->GetURL());
  banners::TrackDisplayEvent(DISPLAY_EVENT_CREATED);
}

void AppBannerDataFetcher::OnDidGetManifest(
    const content::Manifest& manifest) {
  content::WebContents* web_contents = GetWebContents();
  if (!is_active_ || !web_contents) {
    Cancel();
    return;
  }

  if (!IsManifestValid(manifest)) {
    if (!weak_delegate_.get()->OnInvalidManifest(this))
      Cancel();
    return;
  }

  banners::TrackDisplayEvent(DISPLAY_EVENT_BANNER_REQUESTED);

  web_app_data_ = manifest;
  app_title_ = web_app_data_.name.string();

  // Check to see if there is a single service worker controlling this page
  // and the manifest's start url.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartition(
          profile, web_contents->GetSiteInstance());
  DCHECK(storage_partition);

  storage_partition->GetServiceWorkerContext()->CheckHasServiceWorker(
      validated_url_, manifest.start_url,
      base::Bind(&AppBannerDataFetcher::OnDidCheckHasServiceWorker,
                 this));
}

void AppBannerDataFetcher::OnDidCheckHasServiceWorker(
    bool has_service_worker) {
  content::WebContents* web_contents = GetWebContents();
  if (!is_active_ || !web_contents) {
    Cancel();
    return;
  }

  if (has_service_worker) {
    // Create an infobar to promote the manifest's app.
    GURL icon_url =
        ManifestIconSelector::FindBestMatchingIcon(
            web_app_data_.icons,
            ideal_icon_size_,
            gfx::Screen::GetScreenFor(web_contents->GetNativeView()));
    if (!icon_url.is_empty()) {
      FetchIcon(icon_url);
      return;
    }
  } else {
    TrackDisplayEvent(DISPLAY_EVENT_LACKS_SERVICE_WORKER);
  }

  Cancel();
}

void AppBannerDataFetcher::OnFetchComplete(const GURL url,
                                           const SkBitmap* icon) {
  if (is_active_)
    ShowBanner(icon);

  Release();
}

void AppBannerDataFetcher::ShowBanner(const SkBitmap* icon) {
  content::WebContents* web_contents = GetWebContents();
  if (!is_active_ || !web_contents || !icon) {
    Cancel();
    return;
  }

  RecordCouldShowBanner();
  if (!CheckIfShouldShowBanner()) {
    Cancel();
    return;
  }

  app_icon_.reset(new SkBitmap(*icon));
  web_contents->GetMainFrame()->Send(
      new ChromeViewMsg_AppBannerPromptRequest(
          web_contents->GetMainFrame()->GetRoutingID(),
          ++gCurrentRequestID,
          GetBannerType()));
}

void AppBannerDataFetcher::RecordCouldShowBanner() {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW,
      GetCurrentTime());
}

bool AppBannerDataFetcher::CheckIfShouldShowBanner() {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  return AppBannerSettingsHelper::ShouldShowBanner(
      web_contents, validated_url_, GetAppIdentifier(), GetCurrentTime());
}

// static
bool AppBannerDataFetcher::IsManifestValid(
    const content::Manifest& manifest) {
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

}  // namespace banners
