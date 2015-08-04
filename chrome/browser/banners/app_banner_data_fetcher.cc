// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_data_fetcher.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/banners/app_banner_debug_log.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manifest/manifest_icon_selector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"
#include "ui/gfx/screen.h"

namespace {

base::TimeDelta gTimeDeltaForTesting;
int gCurrentRequestID = -1;
const char kPngExtension[] = ".png";

// The requirement for now is an image/png that is at least 144x144.
const int kIconMinimumSize = 144;
bool DoesManifestContainRequiredIcon(const content::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    // The type field is optional. If it isn't present, fall back on checking
    // the src extension, and allow the icon if the extension ends with png.
    if (!base::EqualsASCII(icon.type.string(), "image/png") &&
        !(icon.type.is_null() &&
          base::EndsWith(icon.src.ExtractFileName(), kPngExtension,
                         base::CompareCase::INSENSITIVE_ASCII)))
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
      is_active_(false),
      was_canceled_by_page_(false),
      page_requested_prompt_(false),
      event_request_id_(-1) {
}

void AppBannerDataFetcher::Start(const GURL& validated_url,
                                 ui::PageTransition transition_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  is_active_ = true;
  was_canceled_by_page_ = false;
  page_requested_prompt_ = false;
  transition_type_ = transition_type;
  validated_url_ = validated_url;
  web_contents->GetManifest(
      base::Bind(&AppBannerDataFetcher::OnDidGetManifest, this));
}

void AppBannerDataFetcher::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_active_) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnDecidedWhetherToShow(this, false));
    is_active_ = false;
    was_canceled_by_page_ = false;
    page_requested_prompt_ = false;
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
  if (!details.is_in_page)
    Cancel();
}

bool AppBannerDataFetcher::OnMessageReceived(
    const IPC::Message& message, content::RenderFrameHost* render_frame_host) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(AppBannerDataFetcher, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AppBannerPromptReply,
                        OnBannerPromptReply)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RequestShowAppBanner,
                        OnRequestShowAppBanner)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AppBannerDataFetcher::OnBannerPromptReply(
    content::RenderFrameHost* render_frame_host,
    int request_id,
    blink::WebAppBannerPromptReply reply) {
  content::WebContents* web_contents = GetWebContents();
  if (!CheckFetcherIsStillAlive(web_contents) ||
      request_id != event_request_id_) {
    Cancel();
    return;
  }

  // The renderer might have requested the prompt to be canceled.
  // They may request that it is redisplayed later, so don't Cancel() here.
  // However, log that the cancelation was requested, so Cancel() can be
  // called if a redisplay isn't asked for.
  //
  // The redisplay request may be received before the Cancel prompt reply
  // *after* if it is made before the beforeinstallprompt event handler
  // concludes (e.g. in the event handler itself), so allow the pipeline
  // to continue in this case.
  if (reply == blink::WebAppBannerPromptReply::Cancel &&
      !page_requested_prompt_) {
    was_canceled_by_page_ = true;
    OutputDeveloperNotShownMessage(web_contents, kRendererRequestCancel);
    return;
  }

  // Definitely going to show the banner now.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnDecidedWhetherToShow(this, true));

  ShowBanner(app_icon_.get(), app_title_);
  is_active_ = false;
}

void AppBannerDataFetcher::OnRequestShowAppBanner(
    content::RenderFrameHost* render_frame_host,
    int request_id) {
  if (was_canceled_by_page_) {
    // Simulate an "OK" from the website to restart the banner display pipeline.
    was_canceled_by_page_ = false;
    OnBannerPromptReply(render_frame_host, request_id,
                        blink::WebAppBannerPromptReply::None);
  } else {
    // Log that the prompt request was made for when we get the prompt reply.
    page_requested_prompt_ = true;
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
  bitmap_fetcher_->Init(
      profile->GetRequestContext(),
      std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::LOAD_NORMAL);
  bitmap_fetcher_->Start();
  return true;
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
}

void AppBannerDataFetcher::OnDidGetManifest(
    const content::Manifest& manifest) {
  content::WebContents* web_contents = GetWebContents();
  if (!CheckFetcherIsStillAlive(web_contents)) {
    Cancel();
    return;
  }
  if (manifest.IsEmpty()) {
    OutputDeveloperNotShownMessage(web_contents, kManifestEmpty);
    Cancel();
    return;
  }

  if (manifest.prefer_related_applications &&
      manifest.related_applications.size()) {
    for (const auto& application : manifest.related_applications) {
      std::string platform = base::UTF16ToUTF8(application.platform.string());
      std::string id = base::UTF16ToUTF8(application.id.string());
      if (weak_delegate_->HandleNonWebApp(platform, application.url, id))
        return;
    }
  }

  if (!IsManifestValidForWebApp(manifest, web_contents)) {
    Cancel();
    return;
  }

  web_app_data_ = manifest;
  app_title_ = web_app_data_.name.string();

  if (IsWebAppInstalled(web_contents->GetBrowserContext(),
                        manifest.start_url) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBypassAppBannerEngagementChecks)) {
    OutputDeveloperNotShownMessage(web_contents, kBannerAlreadyAdded);
    Cancel();
    return;
  }

  banners::TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED);

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
  if (!CheckFetcherIsStillAlive(web_contents)) {
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
    OutputDeveloperNotShownMessage(web_contents, kCannotDetermineBestIcon);
  } else {
    TrackDisplayEvent(DISPLAY_EVENT_LACKS_SERVICE_WORKER);
    OutputDeveloperNotShownMessage(web_contents, kNoMatchingServiceWorker);
  }

  Cancel();
}

void AppBannerDataFetcher::OnFetchComplete(const GURL& url,
                                           const SkBitmap* icon) {
  if (is_active_)
    RequestShowBanner(icon);

  Release();
}

bool AppBannerDataFetcher::IsWebAppInstalled(
    content::BrowserContext* browser_context,
    const GURL& start_url) {
  return false;
}

void AppBannerDataFetcher::RequestShowBanner(const SkBitmap* icon) {
  content::WebContents* web_contents = GetWebContents();
  if (!CheckFetcherIsStillAlive(web_contents)) {
    Cancel();
    return;
  }
  if (!icon) {
    OutputDeveloperNotShownMessage(web_contents, kNoIconAvailable);
    Cancel();
    return;
  }

  RecordCouldShowBanner();
  if (!CheckIfShouldShowBanner()) {
    // At this point, the only possible case is that the banner has been added
    // to the homescreen, given all of the other checks that have been made.
    OutputDeveloperNotShownMessage(web_contents, kBannerAlreadyAdded);
    Cancel();
    return;
  }

  app_icon_.reset(new SkBitmap(*icon));
  event_request_id_ = ++gCurrentRequestID;
  web_contents->GetMainFrame()->Send(
      new ChromeViewMsg_AppBannerPromptRequest(
          web_contents->GetMainFrame()->GetRoutingID(),
          event_request_id_,
          GetBannerType()));
}

void AppBannerDataFetcher::RecordCouldShowBanner() {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  AppBannerSettingsHelper::RecordBannerCouldShowEvent(
      web_contents, validated_url_, GetAppIdentifier(),
      GetCurrentTime(), transition_type_);
}

bool AppBannerDataFetcher::CheckIfShouldShowBanner() {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  return AppBannerSettingsHelper::ShouldShowBanner(
      web_contents, validated_url_, GetAppIdentifier(), GetCurrentTime());
}

bool AppBannerDataFetcher::CheckFetcherIsStillAlive(
    content::WebContents* web_contents) {
  if (!is_active_) {
    OutputDeveloperNotShownMessage(web_contents,
                                   kUserNavigatedBeforeBannerShown);
    return false;
  }
  if (!web_contents) {
    return false; // We cannot show a message if |web_contents| is null
  }
  return true;
}

// static
bool AppBannerDataFetcher::IsManifestValidForWebApp(
    const content::Manifest& manifest,
    content::WebContents* web_contents) {
  if (manifest.IsEmpty()) {
    OutputDeveloperNotShownMessage(web_contents, kManifestEmpty);
    return false;
  }
  if (!manifest.start_url.is_valid()) {
    OutputDeveloperNotShownMessage(web_contents, kStartURLNotValid);
    return false;
  }
  if (manifest.name.is_null() && manifest.short_name.is_null()) {
    OutputDeveloperNotShownMessage(web_contents,
                                   kManifestMissingNameOrShortName);
    return false;
  }
  if (!DoesManifestContainRequiredIcon(manifest)) {
    OutputDeveloperNotShownMessage(web_contents, kManifestMissingSuitableIcon);
    return false;
  }
  return true;
}

}  // namespace banners
