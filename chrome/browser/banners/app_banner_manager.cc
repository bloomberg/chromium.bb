// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
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
  InstallableParams params;
  params.check_eligibility = true;
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
void AppBannerManager::SetTotalEngagementToTrigger(double engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

class AppBannerManager::StatusReporter {
 public:
  virtual ~StatusReporter() {}

  // Reports |code| (via a mechanism which depends on the implementation).
  virtual void ReportStatus(content::WebContents* web_contents,
                            InstallableStatusCode code) = 0;
};

}  // namespace banners

namespace {

// Returns a string parameter for a devtools console message corresponding to
// |code|. Returns the empty string if |code| requires no parameter.
std::string GetStatusParam(InstallableStatusCode code) {
  if (code == NO_ACCEPTABLE_ICON || code == MANIFEST_MISSING_SUITABLE_ICON)
    return base::IntToString(InstallableManager::GetMinimumIconSizeInPx());

  return std::string();
}

// Logs installable status codes to the console.
class ConsoleStatusReporter : public banners::AppBannerManager::StatusReporter {
 public:
  // Logs an error message corresponding to |code| to the devtools console
  // attached to |web_contents|.
  void ReportStatus(content::WebContents* web_contents,
                    InstallableStatusCode code) override {
    LogErrorToConsole(web_contents, code, GetStatusParam(code));
  }
};

// Tracks installable status codes via an UMA histogram.
class TrackingStatusReporter
    : public banners::AppBannerManager::StatusReporter {
 public:
  TrackingStatusReporter() : done_(false) {}
  ~TrackingStatusReporter() override { DCHECK(done_); }

  // Records code via an UMA histogram.
  void ReportStatus(content::WebContents* web_contents,
                    InstallableStatusCode code) override {
    // We only increment the histogram once per page load (and only if the
    // banner pipeline is triggered).
    if (!done_ && code != NO_ERROR_DETECTED)
      banners::TrackInstallableStatusCode(code);

    done_ = true;
  }

 private:
  bool done_;
};

class NullStatusReporter : public banners::AppBannerManager::StatusReporter {
 public:
  void ReportStatus(content::WebContents* web_contents,
                    InstallableStatusCode code) override {
    // In general, NullStatusReporter::ReportStatus should not be called.
    // However, it may be called in cases where Stop is called without a
    // preceding call to RequestAppBanner e.g. because the WebContents is being
    // destroyed. In that case, code should always be NO_ERROR_DETECTED.
    DCHECK(code == NO_ERROR_DETECTED);
  }
};

}  // anonymous namespace

namespace banners {

void AppBannerManager::RequestAppBanner(const GURL& validated_url,
                                        bool is_debug_mode) {
  // The only time we should start the pipeline while it is already running is
  // if it's been triggered from devtools.
  if (state_ != State::INACTIVE) {
    DCHECK(is_debug_mode);
    weak_factory_.InvalidateWeakPtrs();
    ResetBindings();
  }

  UpdateState(State::ACTIVE);
  triggered_by_devtools_ = is_debug_mode;

  // We only need to use TrackingStatusReporter if we aren't in debug mode
  // (this avoids skew from testing).
  if (IsDebugMode())
    status_reporter_ = std::make_unique<ConsoleStatusReporter>();
  else
    status_reporter_ = std::make_unique<TrackingStatusReporter>();

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

void AppBannerManager::OnInstall(bool is_native,
                                 blink::WebDisplayMode display) {
  if (!is_native)
    TrackInstallDisplayMode(display);
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

  if (IsExperimentalAppBannersEnabled()) {
    ResetBindings();
    SendBannerPromptRequest();  // Reprompt.
  }
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
      status_reporter_(std::make_unique<NullStatusReporter>()),
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
    Stop(data.error_code);
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
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.has_worker = true;
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
  if (data.has_worker && data.valid_manifest)
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED);

  if (data.error_code != NO_ERROR_DETECTED) {
    if (data.error_code == NO_MATCHING_SERVICE_WORKER)
      TrackDisplayEvent(DISPLAY_EVENT_LACKS_SERVICE_WORKER);

    Stop(data.error_code);
    return;
  }

  DCHECK(data.has_worker && data.valid_manifest);
  DCHECK(!data.primary_icon_url.is_empty());
  DCHECK(data.primary_icon);

  primary_icon_url_ = data.primary_icon_url;
  primary_icon_ = *data.primary_icon;

  // If we triggered the installability check on page load, then it's possible
  // we don't have enough engagement yet. If that's the case, return here but
  // don't call Terminate(). We wait for OnEngagementIncreased to tell us that
  // we should trigger.
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
  DCHECK(status_reporter_);
  status_reporter_->ReportStatus(web_contents, code);
}

void AppBannerManager::ResetCurrentPageData() {
  active_media_players_.clear();
  manifest_ = content::Manifest();
  manifest_url_ = GURL();
  validated_url_ = GURL();
  referrer_.erase();
}

void AppBannerManager::Terminate() {
  if (state_ == State::PENDING_PROMPT)
    TrackBeforeInstallEvent(
        BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_AFTER_PREVENT_DEFAULT);

  if (state_ == State::PENDING_ENGAGEMENT && !has_sufficient_engagement_)
    TrackDisplayEvent(DISPLAY_EVENT_NOT_VISITED_ENOUGH);

  Stop(TerminationCode());
}

InstallableStatusCode AppBannerManager::TerminationCode() const {
  switch (state_) {
    case State::PENDING_PROMPT:
      return RENDERER_CANCELLED;
    case State::PENDING_ENGAGEMENT:
      return has_sufficient_engagement_ ? NO_ERROR_DETECTED
                                        : INSUFFICIENT_ENGAGEMENT;
    case State::FETCHING_MANIFEST:
      return WAITING_FOR_MANIFEST;
    case State::FETCHING_NATIVE_DATA:
      return WAITING_FOR_NATIVE_DATA;
    case State::PENDING_INSTALLABLE_CHECK:
      return WAITING_FOR_INSTALLABLE_CHECK;
    case State::ACTIVE:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
    case State::INACTIVE:
    case State::COMPLETE:
      break;
  }
  return NO_ERROR_DETECTED;
}

void AppBannerManager::Stop(InstallableStatusCode code) {
  ReportStatus(web_contents(), code);

  weak_factory_.InvalidateWeakPtrs();
  ResetBindings();
  UpdateState(State::COMPLETE);
  status_reporter_ = std::make_unique<NullStatusReporter>(),
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
    Terminate();
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
  Terminate();
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

// static
bool AppBannerManager::IsExperimentalAppBannersEnabled() {
  return base::FeatureList::IsEnabled(features::kExperimentalAppBanners);
}

void AppBannerManager::ResetBindings() {
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
    Stop(code);
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
  // request that it is redisplayed later, so don't Terminate() here. However,
  // log that the cancelation was requested, so Terminate() can be called if a
  // redisplay isn't asked for.
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

  // If this is the first time that we are showing the banner for this site,
  // record how long it's been since the first visit.
  if (AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents(), validated_url_, GetAppIdentifier(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW)
          .is_null()) {
    AppBannerSettingsHelper::RecordMinutesFromFirstVisitToShow(
        web_contents(), validated_url_, GetAppIdentifier(), GetCurrentTime());
  }

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
    Stop(NO_GESTURE);
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
