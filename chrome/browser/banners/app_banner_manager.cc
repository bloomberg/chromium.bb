// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

int gCurrentRequestID = -1;
int gTimeDeltaInDaysForTesting = 0;

// Returns |size_in_px| in dp, i.e. divided by the current device scale factor.
int ConvertIconSizeFromPxToDp(int size_in_px) {
  return size_in_px /
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
}

InstallableParams ParamsToGetManifest() {
  return InstallableParams();
}

// Returns an InstallableParams object that requests all checks necessary for
// a web app banner.
InstallableParams ParamsToPerformInstallableCheck(int ideal_icon_size_in_dp,
                                                  int minimum_icon_size_in_dp) {
  InstallableParams params;
  params.ideal_icon_size_in_dp = ideal_icon_size_in_dp;
  params.minimum_icon_size_in_dp = minimum_icon_size_in_dp;
  params.check_installable = true;
  params.fetch_valid_icon = true;

  return params;
}

}  // anonymous namespace

namespace banners {

// static
base::Time AppBannerManager::GetCurrentTime() {
  return base::Time::Now() +
         base::TimeDelta::FromDays(gTimeDeltaInDaysForTesting);
}

// static
void AppBannerManager::SetTimeDeltaForTesting(int days) {
  gTimeDeltaInDaysForTesting = days;
}

// static
void AppBannerManager::SetEngagementWeights(double direct_engagement,
                                            double indirect_engagement) {
  AppBannerSettingsHelper::SetEngagementWeights(direct_engagement,
                                                indirect_engagement);
}

// static
bool AppBannerManager::URLsAreForTheSamePage(const GURL& first,
                                             const GURL& second) {
  return first.GetWithEmptyPath() == second.GetWithEmptyPath() &&
         first.path() == second.path() && first.query() == second.query();
}

void AppBannerManager::RequestAppBanner(const GURL& validated_url,
                                        bool is_debug_mode) {
  // Don't start a redundant banner request. Otherwise, if one is running,
  // invalidate our weak pointers so it terminates.
  content::WebContents* contents = web_contents();
  if (is_active_) {
    if (URLsAreForTheSamePage(validated_url, contents->GetLastCommittedURL()))
      return;
    else
      weak_factory_.InvalidateWeakPtrs();
  }

  is_active_ = true;
  is_debug_mode_ = is_debug_mode;

  // We only need to call ReportStatus if we aren't in debug mode (this avoids
  // skew from testing).
  DCHECK(!need_to_log_status_);
  need_to_log_status_ = !IsDebugMode();

  if (contents->GetMainFrame()->GetParent()) {
    ReportStatus(contents, NOT_IN_MAIN_FRAME);
    Stop();
    return;
  }

  // A secure origin is required to show banners, so exit early if we see the
  // URL is invalid.
  if (!content::IsOriginSecure(validated_url)) {
    ReportStatus(contents, NOT_FROM_SECURE_ORIGIN);
    Stop();
    return;
  }

  if (validated_url_.is_empty())
    validated_url_ = validated_url;

  // Any existing binding is invalid when we request a new banner.
  if (binding_.is_bound())
    binding_.Close();

  manager_->GetData(
      ParamsToGetManifest(),
      base::Bind(&AppBannerManager::OnDidGetManifest, GetWeakPtr()));
}

void AppBannerManager::SendBannerAccepted(int request_id) {
  if (request_id != gCurrentRequestID)
    return;

  DCHECK(event_.is_bound());
  event_->BannerAccepted(GetBannerType());
}

void AppBannerManager::SendBannerDismissed(int request_id) {
  if (request_id != gCurrentRequestID)
    return;

  DCHECK(event_.is_bound());
  event_->BannerDismissed();
}

base::Closure AppBannerManager::FetchWebappSplashScreenImageCallback(
    const std::string& webapp_id) {
  return base::Closure();
}

AppBannerManager::AppBannerManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      SiteEngagementObserver(nullptr),
      manager_(nullptr),
      event_request_id_(-1),
      binding_(this),
      is_active_(false),
      banner_request_queued_(false),
      load_finished_(false),
      was_canceled_by_page_(false),
      page_requested_prompt_(false),
      is_debug_mode_(false),
      need_to_log_status_(false),
      weak_factory_(this) {
  // Ensure the InstallableManager exists since we have a hard dependency on it.
  InstallableManager::CreateForWebContents(web_contents);
  manager_ = InstallableManager::FromWebContents(web_contents);
  DCHECK(manager_);

  AppBannerSettingsHelper::UpdateFromFieldTrial();
}

AppBannerManager::~AppBannerManager() { }

std::string AppBannerManager::GetAppIdentifier() {
  DCHECK(!manifest_.IsEmpty());
  return manifest_.start_url.spec();
}

std::string AppBannerManager::GetBannerType() {
  return "web";
}

std::string AppBannerManager::GetStatusParam(InstallableStatusCode code) {
  if (code == NO_ACCEPTABLE_ICON || code == MANIFEST_MISSING_SUITABLE_ICON) {
    return base::IntToString(InstallableManager::GetMinimumIconSizeInPx());
  }

  return std::string();
}

int AppBannerManager::GetIdealIconSizeInDp() {
  return ConvertIconSizeFromPxToDp(
      InstallableManager::GetMinimumIconSizeInPx());
}

int AppBannerManager::GetMinimumIconSizeInDp() {
  return ConvertIconSizeFromPxToDp(
      InstallableManager::GetMinimumIconSizeInPx());
}

base::WeakPtr<AppBannerManager> AppBannerManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool AppBannerManager::IsDebugMode() const {
  return is_debug_mode_ ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kBypassAppBannerEngagementChecks);
}

bool AppBannerManager::IsWebAppInstalled(
    content::BrowserContext* browser_context,
    const GURL& start_url) {
  return false;
}

void AppBannerManager::OnDidGetManifest(const InstallableData& data) {
  if (data.error_code != NO_ERROR_DETECTED) {
    ReportStatus(web_contents(), data.error_code);
    Stop();
  }

  if (!is_active_)
    return;

  DCHECK(!data.manifest_url.is_empty());
  DCHECK(!data.manifest.IsEmpty());

  manifest_url_ = data.manifest_url;
  manifest_ = data.manifest;
  app_title_ = (manifest_.name.is_null()) ? manifest_.short_name.string()
                                          : manifest_.name.string();

  PerformInstallableCheck();
}

void AppBannerManager::PerformInstallableCheck() {
  if (IsWebAppInstalled(web_contents()->GetBrowserContext(),
                        manifest_.start_url) &&
      !IsDebugMode()) {
    ReportStatus(web_contents(), ALREADY_INSTALLED);
    Stop();
  }

  if (!is_active_)
    return;

  // Fetch and verify the other required information.
  manager_->GetData(ParamsToPerformInstallableCheck(GetIdealIconSizeInDp(),
                                                    GetMinimumIconSizeInDp()),
                    base::Bind(&AppBannerManager::OnDidPerformInstallableCheck,
                               GetWeakPtr()));
}

void AppBannerManager::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  if (data.is_installable)
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED);

  if (data.error_code != NO_ERROR_DETECTED) {
    if (data.error_code == NO_MATCHING_SERVICE_WORKER)
      TrackDisplayEvent(DISPLAY_EVENT_LACKS_SERVICE_WORKER);

    ReportStatus(web_contents(), data.error_code);
    Stop();
  }

  if (!is_active_)
    return;

  DCHECK(data.is_installable);
  DCHECK(!data.icon_url.is_empty());
  DCHECK(data.icon);

  icon_url_ = data.icon_url;
  icon_.reset(new SkBitmap(*data.icon));

  SendBannerPromptRequest();
}

void AppBannerManager::RecordDidShowBanner(const std::string& event_name) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW,
      GetCurrentTime());
  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          event_name,
                                          contents->GetLastCommittedURL());
}

void AppBannerManager::ReportStatus(content::WebContents* web_contents,
                                    InstallableStatusCode code) {
  if (IsDebugMode()) {
    LogErrorToConsole(web_contents, code, GetStatusParam(code));
  } else {
    // Ensure that we haven't yet logged a status code for this page.
    DCHECK(need_to_log_status_);
    TrackInstallableStatusCode(code);
    need_to_log_status_ = false;
  }
}

void AppBannerManager::Stop() {
  if (was_canceled_by_page_ && !page_requested_prompt_) {
    TrackBeforeInstallEvent(
        BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_AFTER_PREVENT_DEFAULT);
    ReportStatus(web_contents(), RENDERER_CANCELLED);
  }

  // In every non-debug run through the banner pipeline, we should have called
  // ReportStatus() and set need_to_log_status_ to false. The only case where
  // we don't is if we're still active and waiting for a callback from the
  // InstallableManager (e.g. the renderer crashes or the browser is shutting
  // down). These situations are explicitly not logged.
  DCHECK(!need_to_log_status_ || is_active_);

  weak_factory_.InvalidateWeakPtrs();
  binding_.Close();
  controller_.reset();
  event_.reset();

  is_active_ = false;
  was_canceled_by_page_ = false;
  page_requested_prompt_ = false;
  need_to_log_status_ = false;
  validated_url_ = GURL();
  referrer_.erase();
}

void AppBannerManager::SendBannerPromptRequest() {
  RecordCouldShowBanner();

  // Given all of the other checks that have been made, the only possible reason
  // for stopping now is that the app has been added to the homescreen.
  if (!IsDebugMode() && !CheckIfShouldShowBanner()) {
    Stop();
    return;
  }

  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_CREATED);
  event_request_id_ = ++gCurrentRequestID;

  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      mojo::GetProxy(&controller_));

  controller_->BannerPromptRequest(
      binding_.CreateInterfacePtrAndBind(), mojo::GetProxy(&event_),
      {GetBannerType()},
      base::Bind(&AppBannerManager::OnBannerPromptReply, GetWeakPtr()));
}

void AppBannerManager::DidStartNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return;

  load_finished_ = false;
  if (AppBannerSettingsHelper::ShouldUseSiteEngagementScore() &&
      GetSiteEngagementService() == nullptr) {
    // Ensure that we are observing the site engagement service on navigation
    // start. This may be the first navigation, or we may have stopped
    // observing if the banner flow was triggered on the previous page.
    SiteEngagementObserver::Observe(SiteEngagementService::Get(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())));
  }
}

void AppBannerManager::DidFinishNavigation(content::NavigationHandle* handle) {
  if (handle->IsInMainFrame() && handle->HasCommitted()) {
    last_transition_type_ = handle->GetPageTransition();
    active_media_players_.clear();
    if (is_active_)
      Stop();
  }
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Don't start the banner flow unless the main frame has finished loading.
  if (render_frame_host->GetParent())
    return;

  load_finished_ = true;
  validated_url_ = validated_url;
  // Start the pipeline immediately if we aren't using engagement, or if 0
  // engagement is required.
  if (!AppBannerSettingsHelper::ShouldUseSiteEngagementScore() ||
      banner_request_queued_ ||
      AppBannerSettingsHelper::HasSufficientEngagement(0)) {
    SiteEngagementObserver::Observe(nullptr);
    banner_request_queued_ = false;

    RequestAppBanner(validated_url, false /* is_debug_mode */);
  }
}

void AppBannerManager::MediaStartedPlaying(const MediaPlayerId& id) {
  active_media_players_.push_back(id);
}

void AppBannerManager::MediaStoppedPlaying(const MediaPlayerId& id) {
  active_media_players_.erase(std::remove(active_media_players_.begin(),
                                          active_media_players_.end(), id),
                              active_media_players_.end());
}

void AppBannerManager::WebContentsDestroyed() {
  Stop();
}

void AppBannerManager::OnEngagementIncreased(content::WebContents* contents,
                                             const GURL& url,
                                             double score) {
  // Only trigger a banner using site engagement if:
  //  1. engagement increased for the web contents which we are attached to; and
  //  2. there are no currently active media players; and
  //  3. we have accumulated sufficient engagement.
  if (web_contents() == contents && active_media_players_.empty() &&
      AppBannerSettingsHelper::HasSufficientEngagement(score)) {
    // Stop observing so we don't double-trigger the banner.
    SiteEngagementObserver::Observe(nullptr);

    if (!load_finished_) {
      // Queue the banner request until the main frame finishes loading.
      banner_request_queued_ = true;
    } else {
      // A banner request performs some simple tests, creates a data fetcher,
      // and starts some asynchronous checks to test installability. It should
      // be safe to start this in response to user input.
      RequestAppBanner(url, false /* is_debug_mode */);
    }
  }
}

void AppBannerManager::RecordCouldShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  AppBannerSettingsHelper::RecordBannerCouldShowEvent(
      contents, validated_url_, GetAppIdentifier(),
      GetCurrentTime(), last_transition_type_);
}

bool AppBannerManager::CheckIfShouldShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  InstallableStatusCode code = AppBannerSettingsHelper::ShouldShowBanner(
      contents, validated_url_, GetAppIdentifier(), GetCurrentTime());
  if (code == NO_ERROR_DETECTED)
    return true;

  switch (code) {
    case ALREADY_INSTALLED:
      banners::TrackDisplayEvent(banners::DISPLAY_EVENT_INSTALLED_PREVIOUSLY);
      break;
    case PREVIOUSLY_BLOCKED:
      banners::TrackDisplayEvent(banners::DISPLAY_EVENT_BLOCKED_PREVIOUSLY);
      break;
    case PREVIOUSLY_IGNORED:
      banners::TrackDisplayEvent(banners::DISPLAY_EVENT_IGNORED_PREVIOUSLY);
      break;
    case INSUFFICIENT_ENGAGEMENT:
      banners::TrackDisplayEvent(banners::DISPLAY_EVENT_NOT_VISITED_ENOUGH);
      break;
    default:
      break;
  }

  // If we are in debug mode, AppBannerSettingsHelper::ShouldShowBanner must
  // return NO_ERROR_DETECTED (bypass flag is set) or we must not have entered
  // this method.
  DCHECK(!IsDebugMode());
  ReportStatus(web_contents(), code);
  return false;
}

void AppBannerManager::OnBannerPromptReply(
    blink::mojom::AppBannerPromptReply reply,
    const std::string& referrer) {
  // We don't need the controller any more, so reset it so the Blink-side object
  // is destroyed.
  controller_.reset();
  content::WebContents* contents = web_contents();

  // The renderer might have requested the prompt to be canceled.
  // They may request that it is redisplayed later, so don't Stop() here.
  // However, log that the cancelation was requested, so Stop() can be
  // called if a redisplay isn't asked for.
  //
  // We use the additional page_requested_prompt_ variable because the redisplay
  // request may be received *before* the Cancel prompt reply (e.g. if redisplay
  // is requested in the beforeinstallprompt event handler).
  referrer_ = referrer;
  if (reply == blink::mojom::AppBannerPromptReply::CANCEL &&
      !page_requested_prompt_) {
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_PREVENT_DEFAULT_CALLED);
    was_canceled_by_page_ = true;
    return;
  }

  // If we haven't yet returned, but either of |was_canceled_by_page_| or
  // |page_requested_prompt_| is true, the page has requested a delayed showing
  // of the prompt. Otherwise, the prompt was never canceled by the page.
  if (was_canceled_by_page_ || page_requested_prompt_) {
    TrackBeforeInstallEvent(
        BEFORE_INSTALL_EVENT_PROMPT_CALLED_AFTER_PREVENT_DEFAULT);
    was_canceled_by_page_ = false;
  } else {
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_NO_ACTION);
  }

  AppBannerSettingsHelper::RecordMinutesFromFirstVisitToShow(
      contents, validated_url_, GetAppIdentifier(), GetCurrentTime());

  DCHECK(!manifest_url_.is_empty());
  DCHECK(!manifest_.IsEmpty());
  DCHECK(!icon_url_.is_empty());
  DCHECK(icon_.get());

  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_COMPLETE);
  ShowBanner();
  is_active_ = false;
}

void AppBannerManager::DisplayAppBanner() {
  if (was_canceled_by_page_) {
    // Simulate a non-canceled OnBannerPromptReply to show the delayed banner.
    // Don't reset |was_canceled_by_page_| yet for metrics purposes.
    OnBannerPromptReply(blink::mojom::AppBannerPromptReply::NONE, referrer_);
  } else {
    // Log that the prompt request was made for when we get the prompt reply.
    page_requested_prompt_ = true;
  }
}

}  // namespace banners
