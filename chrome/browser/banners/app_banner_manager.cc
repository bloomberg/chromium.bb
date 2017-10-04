// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/modules/installation/installation.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

int gTimeDeltaInDaysForTesting = 0;

InstallableParams ParamsToGetManifest() {
  return InstallableParams();
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
void AppBannerManager::SetTotalEngagementToTrigger(double engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

void AppBannerManager::RequestAppBanner(const GURL& validated_url,
                                        bool is_debug_mode) {
  content::WebContents* contents = web_contents();

  // The only time we should start the pipeline while it is already running is
  // if it's been triggered from devtools.
  if (state_ != State::INACTIVE) {
    DCHECK(is_debug_mode);
    ResetBindings();
  }

  UpdateState(State::ACTIVE);
  triggered_by_devtools_ = is_debug_mode;

  // We only need to call ReportStatus if we aren't in debug mode (this avoids
  // skew from testing).
  DCHECK(!need_to_log_status_);
  need_to_log_status_ = !IsDebugMode();

  // Exit if this is an incognito window, non-main frame, or insecure context.
  InstallableStatusCode code = NO_ERROR_DETECTED;
  if (Profile::FromBrowserContext(contents->GetBrowserContext())
          ->IsOffTheRecord()) {
    code = IN_INCOGNITO;
  } else if (contents->GetMainFrame()->GetParent()) {
    code = NOT_IN_MAIN_FRAME;
  } else if (!InstallableManager::IsContentSecure(contents)) {
    code = NOT_FROM_SECURE_ORIGIN;
  }

  if (code != NO_ERROR_DETECTED) {
    StopWithCode(code);
    return;
  }

  if (validated_url_.is_empty())
    validated_url_ = validated_url;

  // Any existing binding is invalid when we request a new banner.
  if (binding_.is_bound())
    binding_.Close();

  UpdateState(State::FETCHING_MANIFEST);
  manager_->GetData(
      ParamsToGetManifest(),
      base::Bind(&AppBannerManager::OnDidGetManifest, GetWeakPtr()));
}

void AppBannerManager::OnInstall() {
  blink::mojom::InstallationServicePtr installation_service;
  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      mojo::MakeRequest(&installation_service));
  DCHECK(installation_service);
  installation_service->OnInstall();
}

void AppBannerManager::SendBannerAccepted() {
  if (event_.is_bound())
    event_->BannerAccepted(GetBannerType());
}

void AppBannerManager::SendBannerDismissed() {
  if (event_.is_bound())
    event_->BannerDismissed();
}

base::WeakPtr<AppBannerManager> AppBannerManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

AppBannerManager::AppBannerManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      SiteEngagementObserver(SiteEngagementService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      state_(State::INACTIVE),
      manager_(InstallableManager::FromWebContents(web_contents)),
      binding_(this),
      has_sufficient_engagement_(false),
      load_finished_(false),
      triggered_by_devtools_(false),
      need_to_log_status_(false),
      weak_factory_(this) {
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

bool AppBannerManager::HasSufficientEngagement() const {
  return has_sufficient_engagement_ || IsDebugMode();
}

bool AppBannerManager::IsDebugMode() const {
  return triggered_by_devtools_ ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kBypassAppBannerEngagementChecks);
}

bool AppBannerManager::IsWebAppInstalled(
    content::BrowserContext* browser_context,
    const GURL& start_url,
    const GURL& manifest_url) {
  return false;
}

void AppBannerManager::OnDidGetManifest(const InstallableData& data) {
  UpdateState(State::ACTIVE);
  if (data.error_code != NO_ERROR_DETECTED) {
    StopWithCode(data.error_code);
    return;
  }

  DCHECK(!data.manifest_url.is_empty());
  DCHECK(!data.manifest.IsEmpty());

  manifest_url_ = data.manifest_url;
  manifest_ = data.manifest;

  PerformInstallableCheck();
}

InstallableParams AppBannerManager::ParamsToPerformInstallableCheck() {
  InstallableParams params;
  params.check_installable = true;
  params.fetch_valid_primary_icon = true;

  // Don't wait for the service worker if this was triggered from devtools.
  params.wait_for_worker = !triggered_by_devtools_;

  return params;
}

void AppBannerManager::PerformInstallableCheck() {
  if (!CheckIfShouldShowBanner())
    return;

  // Fetch and verify the other required information.
  UpdateState(State::PENDING_INSTALLABLE_CHECK);
  manager_->GetData(ParamsToPerformInstallableCheck(),
                    base::Bind(&AppBannerManager::OnDidPerformInstallableCheck,
                               GetWeakPtr()));
}

void AppBannerManager::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  UpdateState(State::ACTIVE);
  if (data.is_installable)
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED);

  if (data.error_code != NO_ERROR_DETECTED) {
    if (data.error_code == NO_MATCHING_SERVICE_WORKER)
      TrackDisplayEvent(DISPLAY_EVENT_LACKS_SERVICE_WORKER);

    StopWithCode(data.error_code);
    return;
  }

  DCHECK(data.is_installable);
  DCHECK(!data.primary_icon_url.is_empty());
  DCHECK(data.primary_icon);

  primary_icon_url_ = data.primary_icon_url;
  primary_icon_ = *data.primary_icon;

  // If we triggered the installability check on page load, then it's possible
  // we don't have enough engagement yet. If that's the case, return here but
  // don't call Stop(). We wait for OnEngagementIncreased to tell us that we
  // should trigger.
  if (!HasSufficientEngagement()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

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

void AppBannerManager::ResetCurrentPageData() {
  active_media_players_.clear();
  manifest_ = content::Manifest();
  manifest_url_ = GURL();
  validated_url_ = GURL();
  referrer_.erase();
}

void AppBannerManager::Stop() {
  InstallableStatusCode code = NO_ERROR_DETECTED;
  switch (state_) {
    case State::PENDING_PROMPT:
      TrackBeforeInstallEvent(
          BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_AFTER_PREVENT_DEFAULT);
      code = RENDERER_CANCELLED;
      break;
    case State::PENDING_ENGAGEMENT:
      if (!has_sufficient_engagement_) {
        TrackDisplayEvent(DISPLAY_EVENT_NOT_VISITED_ENOUGH);
        code = INSUFFICIENT_ENGAGEMENT;
      }
      break;
    case State::FETCHING_MANIFEST:
      code = WAITING_FOR_MANIFEST;
      break;
    case State::FETCHING_NATIVE_DATA:
      code = WAITING_FOR_NATIVE_DATA;
      break;
    case State::PENDING_INSTALLABLE_CHECK:
      code = WAITING_FOR_INSTALLABLE_CHECK;
      break;
    case State::ACTIVE:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
    case State::INACTIVE:
    case State::COMPLETE:
      break;
  }
  StopWithCode(code);
}

void AppBannerManager::StopWithCode(InstallableStatusCode code) {
  if (code != NO_ERROR_DETECTED)
    ReportStatus(web_contents(), code);

  // In every non-debug run through the banner pipeline, we should have called
  // ReportStatus() and set need_to_log_status_ to false. The only case where
  // we don't is if we're still running and aren't blocked on the network. When
  // running and blocked on the network the state should be logged.
  DCHECK(!need_to_log_status_ || (IsRunning() && !IsWaitingForData()));

  ResetBindings();
  UpdateState(State::COMPLETE);
  need_to_log_status_ = false;
  has_sufficient_engagement_ = false;
}

void AppBannerManager::SendBannerPromptRequest() {
  RecordCouldShowBanner();

  UpdateState(State::SENDING_EVENT);
  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_CREATED);

  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      mojo::MakeRequest(&controller_));

  blink::mojom::AppBannerServicePtr banner_proxy;
  binding_.Bind(mojo::MakeRequest(&banner_proxy));
  controller_->BannerPromptRequest(
      std::move(banner_proxy), mojo::MakeRequest(&event_), {GetBannerType()},
      base::Bind(&AppBannerManager::OnBannerPromptReply, GetWeakPtr()));
}

void AppBannerManager::UpdateState(State state) {
  state_ = state;
}

void AppBannerManager::DidStartNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame() || handle->IsSameDocument())
    return;

  if (state_ != State::COMPLETE && state_ != State::INACTIVE)
    Stop();
  UpdateState(State::INACTIVE);
  load_finished_ = false;
  has_sufficient_engagement_ = false;
}

void AppBannerManager::DidFinishNavigation(content::NavigationHandle* handle) {
  if (handle->IsInMainFrame() && handle->HasCommitted() &&
      !handle->IsSameDocument()) {
    ResetCurrentPageData();
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

  // If we already have enough engagement, or require no engagement to trigger
  // the banner, the rest of the banner pipeline should operate as if the
  // engagement threshold has been met.
  if (AppBannerSettingsHelper::HasSufficientEngagement(0) ||
      AppBannerSettingsHelper::HasSufficientEngagement(
          GetSiteEngagementService()->GetScore(validated_url))) {
    has_sufficient_engagement_ = true;
  }

  // Start the pipeline immediately if we pass (or bypass) the engagement check,
  // or if the feature to run the installability check on page load is enabled.
  if (state_ == State::INACTIVE &&
      (has_sufficient_engagement_ ||
       base::FeatureList::IsEnabled(
           features::kCheckInstallabilityForBannerOnLoad))) {
    RequestAppBanner(validated_url, false /* is_debug_mode */);
  }
}

void AppBannerManager::MediaStartedPlaying(const MediaPlayerInfo& media_info,
                                           const MediaPlayerId& id) {
  active_media_players_.push_back(id);
}

void AppBannerManager::MediaStoppedPlaying(const MediaPlayerInfo& media_info,
                                           const MediaPlayerId& id) {
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
    has_sufficient_engagement_ = true;

    if (state_ == State::PENDING_ENGAGEMENT) {
      // We have already finished the installability eligibility checks. Proceed
      // directly to sending the banner prompt request.
      UpdateState(State::ACTIVE);
      SendBannerPromptRequest();
    } else if (load_finished_ && state_ == State::INACTIVE) {
      // This performs some simple tests and starts async checks to test
      // installability. It should be safe to start in response to user input.
      // Don't call if we're already working on processing a banner request.
      RequestAppBanner(url, false /* is_debug_mode */);
    }
  }
}

bool AppBannerManager::IsRunning() const {
  switch (state_) {
    case State::INACTIVE:
    case State::PENDING_PROMPT:
    case State::PENDING_ENGAGEMENT:
    case State::COMPLETE:
      return false;
    case State::ACTIVE:
    case State::FETCHING_MANIFEST:
    case State::FETCHING_NATIVE_DATA:
    case State::PENDING_INSTALLABLE_CHECK:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
      return true;
  }
  return false;
}

bool AppBannerManager::IsWaitingForData() const {
  switch (state_) {
    case State::INACTIVE:
    case State::ACTIVE:
    case State::PENDING_ENGAGEMENT:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
    case State::PENDING_PROMPT:
    case State::COMPLETE:
      return false;
    case State::FETCHING_MANIFEST:
    case State::FETCHING_NATIVE_DATA:
    case State::PENDING_INSTALLABLE_CHECK:
      return true;
  }
  return false;
}

// static
bool AppBannerManager::IsExperimentalAppBannersEnabled() {
  return base::FeatureList::IsEnabled(features::kExperimentalAppBanners);
}

void AppBannerManager::ResetBindings() {
  weak_factory_.InvalidateWeakPtrs();
  binding_.Close();
  controller_.reset();
  event_.reset();
}

void AppBannerManager::RecordCouldShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, GetCurrentTime());
}

bool AppBannerManager::CheckIfShouldShowBanner() {
  if (IsDebugMode())
    return true;

  // Check whether we are permitted to show the banner. If we have already
  // added this site to homescreen, or if the banner has been shown too
  // recently, prevent the banner from being shown.
  content::WebContents* contents = web_contents();
  InstallableStatusCode code = AppBannerSettingsHelper::ShouldShowBanner(
      contents, validated_url_, GetAppIdentifier(), GetCurrentTime());

  if (code == NO_ERROR_DETECTED &&
      IsWebAppInstalled(contents->GetBrowserContext(), manifest_.start_url,
                        manifest_url_)) {
    code = ALREADY_INSTALLED;
  }

  if (code != NO_ERROR_DETECTED) {
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
      case PACKAGE_NAME_OR_START_URL_EMPTY:
        break;
      default:
        NOTREACHED();
    }
    StopWithCode(code);
    return false;
  }
  return true;
}

void AppBannerManager::OnBannerPromptReply(
    blink::mojom::AppBannerPromptReply reply,
    const std::string& referrer) {
  // We don't need the controller any more, so reset it so the Blink-side object
  // is destroyed.
  controller_.reset();

  // The renderer might have requested the prompt to be canceled. They may
  // request that it is redisplayed later, so don't Stop() here. However, log
  // that the cancelation was requested, so Stop() can be called if a redisplay
  // isn't asked for.
  //
  // If the redisplay request has not been received already, we stop here and
  // wait for the prompt function to be called. If the redisplay request has
  // already been received before cancel was sent (e.g. if redisplay was
  // requested in the beforeinstallprompt event handler), we keep going and show
  // the banner immediately.
  referrer_ = referrer;
  if (reply == blink::mojom::AppBannerPromptReply::CANCEL)
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_PREVENT_DEFAULT_CALLED);

  if (IsExperimentalAppBannersEnabled() ||
      reply == blink::mojom::AppBannerPromptReply::CANCEL) {
    if (state_ == State::SENDING_EVENT) {
      UpdateState(State::PENDING_PROMPT);
      return;
    }
    DCHECK_EQ(State::SENDING_EVENT_GOT_EARLY_PROMPT, state_);
  }

  if (!IsExperimentalAppBannersEnabled())
    ShowBanner();
}

void AppBannerManager::ShowBanner() {
  // If we are still in the SENDING_EVENT state, the prompt was never canceled
  // by the page. Otherwise the page requested a delayed showing of the prompt.
  if (state_ == State::SENDING_EVENT) {
    // In the experimental flow, the banner is only shown if the site explicitly
    // requests it to be shown.
    DCHECK(!IsExperimentalAppBannersEnabled());
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_NO_ACTION);
  } else {
    TrackBeforeInstallEvent(
        BEFORE_INSTALL_EVENT_PROMPT_CALLED_AFTER_PREVENT_DEFAULT);
  }

  AppBannerSettingsHelper::RecordMinutesFromFirstVisitToShow(
      web_contents(), validated_url_, GetAppIdentifier(), GetCurrentTime());

  DCHECK(!manifest_url_.is_empty());
  DCHECK(!manifest_.IsEmpty());
  DCHECK(!primary_icon_url_.is_empty());
  DCHECK(!primary_icon_.drawsNothing());

  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_COMPLETE);
  ShowBannerUi();
  UpdateState(State::COMPLETE);
}

void AppBannerManager::DisplayAppBanner(bool user_gesture) {
  if (IsExperimentalAppBannersEnabled() && !user_gesture) {
    StopWithCode(NO_GESTURE);
    return;
  }

  if (state_ == State::PENDING_PROMPT) {
    ShowBanner();
  } else if (state_ == State::SENDING_EVENT) {
    // Log that the prompt request was made for when we get the prompt reply.
    UpdateState(State::SENDING_EVENT_GOT_EARLY_PROMPT);
  }
}

}  // namespace banners
