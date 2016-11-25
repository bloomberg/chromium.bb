// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingBlockingPage class.

#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_controller_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/safe_browsing/threat_details.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;
using content::BrowserThread;
using content::InterstitialPage;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace safe_browsing {

namespace {

// For malware interstitial pages, we link the problematic URL to Google's
// diagnostic page.
#if defined(GOOGLE_CHROME_BUILD)
const char kSbDiagnosticUrl[] =
    "https://www.google.com/safebrowsing/diagnostic?site=%s&client=googlechrome";
#else
const char kSbDiagnosticUrl[] =
    "https://www.google.com/safebrowsing/diagnostic?site=%s&client=chromium";
#endif

// URL for the Help Center article on Safe Browsing warnings.
const char kLearnMore[] = "https://support.google.com/chrome/answer/99020";

// After a safe browsing interstitial where the user opted-in to the report
// but clicked "proceed anyway", we delay the call to
// ThreatDetails::FinishCollection() by this much time (in
// milliseconds).
const int64_t kThreatDetailsProceedDelayMilliSeconds = 3000;

// Constants for the Experience Sampling instrumentation.
const char kEventNameMalware[] = "safebrowsing_interstitial_";
const char kEventNameHarmful[] = "harmful_interstitial_";
const char kEventNamePhishing[] = "phishing_interstitial_";
const char kEventNameOther[] = "safebrowsing_other_interstitial_";

// Constants for the V4 phishing string upgrades.
const char kReportPhishingErrorUrl[] =
    "https://www.google.com/safebrowsing/report_error/";

base::LazyInstance<SafeBrowsingBlockingPage::UnsafeResourceMap>
    g_unsafe_resource_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
SafeBrowsingBlockingPageFactory* SafeBrowsingBlockingPage::factory_ = NULL;

// The default SafeBrowsingBlockingPageFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingBlockingPageFactoryImpl
    : public SafeBrowsingBlockingPageFactory {
 public:
  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    return new SafeBrowsingBlockingPage(ui_manager, web_contents,
                                        main_frame_url, unsafe_resources);
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<
      SafeBrowsingBlockingPageFactoryImpl>;

  SafeBrowsingBlockingPageFactoryImpl() { }

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageFactoryImpl);
};

static base::LazyInstance<SafeBrowsingBlockingPageFactoryImpl>
    g_safe_browsing_blocking_page_factory_impl = LAZY_INSTANCE_INITIALIZER;

// static
content::InterstitialPageDelegate::TypeID
    SafeBrowsingBlockingPage::kTypeForTesting =
        &SafeBrowsingBlockingPage::kTypeForTesting;

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources)
    : SecurityInterstitialPage(
          web_contents,
          unsafe_resources[0].url,
          CreateMetricsHelper(web_contents, unsafe_resources)),
      threat_details_proceed_delay_ms_(kThreatDetailsProceedDelayMilliSeconds),
      ui_manager_(ui_manager),
      is_main_frame_load_blocked_(IsMainPageLoadBlocked(unsafe_resources)),
      main_frame_url_(main_frame_url),
      unsafe_resources_(unsafe_resources),
      proceeded_(false),
      interstitial_reason_(GetInterstitialReason(unsafe_resources)) {
  controller()->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::SHOW);
  controller()->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);
  if (IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled)) {
    controller()->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEEDING_DISABLED);
  }

  if (!is_main_frame_load_blocked_) {
    navigation_entry_index_to_remove_ =
        web_contents->GetController().GetLastCommittedEntryIndex();
  } else {
    navigation_entry_index_to_remove_ = -1;
  }

  // Start computing threat details. They will be sent only
  // if the user opts-in on the blocking page later.
  // If there's more than one malicious resources, it means the user
  // clicked through the first warning, so we don't prepare additional
  // reports.
  if (unsafe_resources.size() == 1 &&
      ShouldReportThreatDetails(unsafe_resources[0].threat_type) &&
      threat_details_.get() == NULL && CanShowThreatDetailsOption()) {
    threat_details_ = ThreatDetails::NewThreatDetails(ui_manager_, web_contents,
                                                      unsafe_resources[0]);
  }
}

bool SafeBrowsingBlockingPage::ShouldReportThreatDetails(
    SBThreatType threat_type) {
  return threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL ||
         threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
}

bool SafeBrowsingBlockingPage::CanShowThreatDetailsOption() {
  return (!web_contents()->GetBrowserContext()->IsOffTheRecord() &&
          main_frame_url_.SchemeIs(url::kHttpScheme) &&
          IsPrefEnabled(prefs::kSafeBrowsingExtendedReportingOptInAllowed));
}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {
}

void SafeBrowsingBlockingPage::CommandReceived(const std::string& page_cmd) {
  if (page_cmd == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int command = 0;
  bool retval = base::StringToInt(page_cmd, &command);
  DCHECK(retval) << page_cmd;

  switch (command) {
    case security_interstitials::CMD_DO_REPORT: {
      // User enabled SB Extended Reporting via the checkbox.
      controller()->SetReportingPreference(true);
      break;
    }
    case security_interstitials::CMD_DONT_REPORT: {
      // User disabled SB Extended Reporting via the checkbox.
      controller()->SetReportingPreference(false);
      break;
    }
    case security_interstitials::CMD_OPEN_HELP_CENTER: {
      // User pressed "Learn more".
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
      GURL learn_more_url(kLearnMore);
      learn_more_url = google_util::AppendGoogleLocaleParam(
          learn_more_url, g_browser_process->GetApplicationLocale());
      OpenURLParams params(learn_more_url, Referrer(),
                           WindowOpenDisposition::CURRENT_TAB,
                           ui::PAGE_TRANSITION_LINK, false);
      web_contents()->OpenURL(params);
      break;
    }
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY: {
      // User pressed on the SB Extended Reporting "privacy policy" link.
      controller()->OpenExtendedReportingPrivacyPolicy();
      break;
    }
    case security_interstitials::CMD_PROCEED: {
      // User pressed on the button to proceed.
      if (!IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled)) {
        metrics_helper()->RecordUserDecision(
            security_interstitials::MetricsHelper::PROCEED);
        interstitial_page()->Proceed();
        // |this| has been deleted after Proceed() returns.
        break;
      }
      // If the user can't proceed, fall through to CMD_DONT_PROCEED.
    }
    case security_interstitials::CMD_DONT_PROCEED: {
      // User pressed on the button to return to safety.
      // Don't record the user action here because there are other ways of
      // triggering DontProceed, like clicking the back button.
      if (is_main_frame_load_blocked_) {
        // If the load is blocked, we want to close the interstitial and discard
        // the pending entry.
        interstitial_page()->DontProceed();
        // |this| has been deleted after DontProceed() returns.
        break;
      }

      // Otherwise the offending entry has committed, and we need to go back or
      // to a safe page.  We will close the interstitial when that page commits.
      if (web_contents()->GetController().CanGoBack()) {
        web_contents()->GetController().GoBack();
      } else {
        web_contents()->GetController().LoadURL(
            GURL(chrome::kChromeUINewTabURL),
            content::Referrer(),
            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
            std::string());
      }
      break;
    }
    case security_interstitials::CMD_OPEN_DIAGNOSTIC: {
      // User wants to see why this page is blocked.
      const UnsafeResource& unsafe_resource = unsafe_resources_[0];
      std::string bad_url_spec = unsafe_resource.url.spec();
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_DIAGNOSTIC);
      std::string diagnostic =
          base::StringPrintf(kSbDiagnosticUrl,
              net::EscapeQueryParamValue(bad_url_spec, true).c_str());
      GURL diagnostic_url(diagnostic);
      diagnostic_url = google_util::AppendGoogleLocaleParam(
          diagnostic_url, g_browser_process->GetApplicationLocale());
      DCHECK(unsafe_resource.threat_type == SB_THREAT_TYPE_URL_MALWARE ||
             unsafe_resource.threat_type ==
                 SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL ||
             unsafe_resource.threat_type == SB_THREAT_TYPE_URL_UNWANTED);
      OpenURLParams params(diagnostic_url, Referrer(),
                           WindowOpenDisposition::CURRENT_TAB,
                           ui::PAGE_TRANSITION_LINK, false);
      web_contents()->OpenURL(params);
      break;
    }
    case security_interstitials::CMD_SHOW_MORE_SECTION: {
      // User has opened up the hidden text.
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    }
    case security_interstitials::CMD_REPORT_PHISHING_ERROR: {
      // User wants to report a phishing error.
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::REPORT_PHISHING_ERROR);
      GURL phishing_error_url(kReportPhishingErrorUrl);
      phishing_error_url = google_util::AppendGoogleLocaleParam(
          phishing_error_url, g_browser_process->GetApplicationLocale());
      OpenURLParams params(phishing_error_url, Referrer(),
                           WindowOpenDisposition::CURRENT_TAB,
                           ui::PAGE_TRANSITION_LINK, false);
      web_contents()->OpenURL(params);
      break;
    }
    case security_interstitials::CMD_OPEN_WHITEPAPER: {
      controller()->OpenExtendedReportingWhitepaper();
      break;
    }
  }
}

void SafeBrowsingBlockingPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, profile, web_contents());
}

void SafeBrowsingBlockingPage::OnProceed() {
  proceeded_ = true;
  // Send the threat details, if we opted to.
  FinishThreatDetails(threat_details_proceed_delay_ms_, true, /* did_proceed */
                      controller()->metrics_helper()->NumVisits());

  ui_manager_->OnBlockingPageDone(unsafe_resources_, true, web_contents(),
                                  main_frame_url_);

  // Check to see if some new notifications of unsafe resources have been
  // received while we were showing the interstitial.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  UnsafeResourceMap::iterator iter = unsafe_resource_map->find(web_contents());
  SafeBrowsingBlockingPage* blocking_page = NULL;
  if (iter != unsafe_resource_map->end() && !iter->second.empty()) {
    // All queued unsafe resources should be for the same page:
    content::NavigationEntry* entry =
        iter->second[0].GetNavigationEntryForResource();
    // Build an interstitial for all the unsafe resources notifications.
    // Don't show it now as showing an interstitial while an interstitial is
    // already showing would cause DontProceed() to be invoked.
    blocking_page = factory_->CreateSafeBrowsingPage(
        ui_manager_, web_contents(), entry ? entry->GetURL() : GURL(),
        iter->second);
    unsafe_resource_map->erase(iter);
  }

  // Now that this interstitial is gone, we can show the new one.
  if (blocking_page)
    blocking_page->Show();
}

content::InterstitialPageDelegate::TypeID
SafeBrowsingBlockingPage::GetTypeForTesting() const {
  return SafeBrowsingBlockingPage::kTypeForTesting;
}

bool SafeBrowsingBlockingPage::ShouldCreateNewNavigation() const {
  return is_main_frame_load_blocked_;
}

void SafeBrowsingBlockingPage::OnDontProceed() {
  // We could have already called Proceed(), in which case we must not notify
  // the SafeBrowsingUIManager again, as the client has been deleted.
  if (proceeded_)
    return;

  if (!IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled)) {
    controller()->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
  }

  // Send the malware details, if we opted to.
  FinishThreatDetails(0, false /* did_proceed */,
                      controller()->metrics_helper()->NumVisits());  // No delay

  ui_manager_->OnBlockingPageDone(unsafe_resources_, false, web_contents(),
                                  main_frame_url_);

  // The user does not want to proceed, clear the queued unsafe resources
  // notifications we received while the interstitial was showing.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  UnsafeResourceMap::iterator iter = unsafe_resource_map->find(web_contents());
  if (iter != unsafe_resource_map->end() && !iter->second.empty()) {
    ui_manager_->OnBlockingPageDone(iter->second, false, web_contents(),
                                    main_frame_url_);
    unsafe_resource_map->erase(iter);
  }

  // We don't remove the navigation entry if the tab is being destroyed as this
  // would trigger a navigation that would cause trouble as the render view host
  // for the tab has by then already been destroyed.  We also don't delete the
  // current entry if it has been committed again, which is possible on a page
  // that had a subresource warning.
  int last_committed_index =
      web_contents()->GetController().GetLastCommittedEntryIndex();
  if (navigation_entry_index_to_remove_ != -1 &&
      navigation_entry_index_to_remove_ != last_committed_index &&
      !web_contents()->IsBeingDestroyed()) {
    CHECK(web_contents()->GetController().RemoveEntryAtIndex(
        navigation_entry_index_to_remove_));
    navigation_entry_index_to_remove_ = -1;
  }
}

void SafeBrowsingBlockingPage::FinishThreatDetails(int64_t delay_ms,
                                                   bool did_proceed,
                                                   int num_visits) {
  if (threat_details_.get() == NULL)
    return;  // Not all interstitials have threat details (eg., incognito mode).

  const bool enabled =
      IsExtendedReportingEnabled(*profile()->GetPrefs()) &&
      IsPrefEnabled(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
  if (!enabled)
    return;

  controller()->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  // Finish the malware details collection, send it over.
  BrowserThread::PostDelayedTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ThreatDetails::FinishCollection, threat_details_,
                 did_proceed, num_visits),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

// static
SafeBrowsingBlockingPage::UnsafeResourceMap*
    SafeBrowsingBlockingPage::GetUnsafeResourcesMap() {
  return g_unsafe_resource_map.Pointer();
}

// static
SafeBrowsingBlockingPage* SafeBrowsingBlockingPage::CreateBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResource& unsafe_resource) {
  std::vector<UnsafeResource> resources;
  resources.push_back(unsafe_resource);
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_safe_browsing_blocking_page_factory_impl.Pointer();
  return factory_->CreateSafeBrowsingPage(ui_manager, web_contents,
                                          main_frame_url, resources);
}

// static
void SafeBrowsingBlockingPage::ShowBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    const UnsafeResource& unsafe_resource) {
  DVLOG(1) << __func__ << " " << unsafe_resource.url.spec();
  WebContents* web_contents = unsafe_resource.web_contents_getter.Run();

  InterstitialPage* interstitial =
      InterstitialPage::GetInterstitialPage(web_contents);
  if (interstitial && !unsafe_resource.is_subresource) {
    // There is already an interstitial showing and we are about to display a
    // new one for the main frame. Just hide the current one, it is now
    // irrelevent
    interstitial->DontProceed();
    interstitial = NULL;
  }

  if (!interstitial) {
    // There are no interstitial currently showing in that tab, go ahead and
    // show this interstitial.
    content::NavigationEntry* entry =
        unsafe_resource.GetNavigationEntryForResource();
    SafeBrowsingBlockingPage* blocking_page =
        CreateBlockingPage(ui_manager, web_contents,
                           entry ? entry->GetURL() : GURL(), unsafe_resource);
    blocking_page->Show();
    return;
  }

  // This is an interstitial for a page's resource, let's queue it.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  (*unsafe_resource_map)[web_contents].push_back(unsafe_resource);
}

// static
bool SafeBrowsingBlockingPage::IsMainPageLoadBlocked(
    const UnsafeResourceList& unsafe_resources) {
  // If there is more than one unsafe resource, the main page load must not be
  // blocked. Otherwise, check if the one resource is.
  return unsafe_resources.size() == 1 &&
         unsafe_resources[0].IsMainPageLoadBlocked();
}

// static
std::string SafeBrowsingBlockingPage::GetMetricPrefix(
    const UnsafeResourceList& unsafe_resources,
    SBInterstitialReason interstitial_reason) {
  bool primary_subresource = unsafe_resources[0].is_subresource;
  switch (interstitial_reason) {
    case SB_REASON_MALWARE:
      return primary_subresource ? "malware_subresource" : "malware";
    case SB_REASON_HARMFUL:
      return primary_subresource ? "harmful_subresource" : "harmful";
    case SB_REASON_PHISHING:
      ThreatPatternType threat_pattern_type =
          unsafe_resources[0].threat_metadata.threat_pattern_type;
      if (threat_pattern_type == ThreatPatternType::PHISHING ||
          threat_pattern_type == ThreatPatternType::NONE)
        return primary_subresource ? "phishing_subresource" : "phishing";
      else if (threat_pattern_type == ThreatPatternType::SOCIAL_ENGINEERING_ADS)
        return primary_subresource ? "social_engineering_ads_subresource"
                                   : "social_engineering_ads";
      else if (threat_pattern_type ==
               ThreatPatternType::SOCIAL_ENGINEERING_LANDING)
        return primary_subresource ? "social_engineering_landing_subresource"
                                   : "social_engineering_landing";
  }
  NOTREACHED();
  return "unkown_metric_prefix";
}

// We populate a parallel set of metrics to differentiate some threat sources.
// static
std::string SafeBrowsingBlockingPage::GetExtraMetricsSuffix(
    const UnsafeResourceList& unsafe_resources) {
  switch (unsafe_resources[0].threat_source) {
    case safe_browsing::ThreatSource::DATA_SAVER:
      return "from_data_saver";
    case safe_browsing::ThreatSource::REMOTE:
    case safe_browsing::ThreatSource::LOCAL_PVER3:
      // REMOTE and LOCAL_PVER3 can be distinguished in the logs
      // by platform type: Remote is mobile, local_pver3 is desktop.
      return "from_device";
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      return "from_device_v4";
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      return "from_client_side_detection";
    case safe_browsing::ThreatSource::UNKNOWN:
      break;
  }
  NOTREACHED();
  return std::string();
}

// static
std::string SafeBrowsingBlockingPage::GetRapporPrefix(
    SBInterstitialReason interstitial_reason) {
  switch (interstitial_reason) {
    case SB_REASON_MALWARE:
      return "malware2";
    case SB_REASON_HARMFUL:
      return "harmful2";
    case SB_REASON_PHISHING:
      return "phishing2";
  }
  NOTREACHED();
  return std::string();
}

// static
std::string SafeBrowsingBlockingPage::GetDeprecatedRapporPrefix(
    SBInterstitialReason interstitial_reason) {
  switch (interstitial_reason) {
    case SB_REASON_MALWARE:
      return "malware";
    case SB_REASON_HARMFUL:
      return "harmful";
    case SB_REASON_PHISHING:
      return "phishing";
  }
  NOTREACHED();
  return std::string();
}

// static
std::string SafeBrowsingBlockingPage::GetSamplingEventName(
    SBInterstitialReason interstitial_reason) {
  switch (interstitial_reason) {
    case SB_REASON_MALWARE:
      return kEventNameMalware;
    case SB_REASON_HARMFUL:
      return kEventNameHarmful;
    case SB_REASON_PHISHING:
      return kEventNamePhishing;
    default:
      return kEventNameOther;
  }
}

// static
SafeBrowsingBlockingPage::SBInterstitialReason
SafeBrowsingBlockingPage::GetInterstitialReason(
    const UnsafeResourceList& unsafe_resources) {
  bool malware = false;
  bool harmful = false;
  bool phishing = false;
  for (UnsafeResourceList::const_iterator iter = unsafe_resources.begin();
       iter != unsafe_resources.end(); ++iter) {
    const SafeBrowsingUIManager::UnsafeResource& resource = *iter;
    safe_browsing::SBThreatType threat_type = resource.threat_type;
    if (threat_type == SB_THREAT_TYPE_URL_MALWARE ||
        threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL) {
      malware = true;
    } else if (threat_type == SB_THREAT_TYPE_URL_UNWANTED) {
      harmful = true;
    } else {
      DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING ||
             threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL);
      phishing = true;
    }
  }
  DCHECK(phishing || malware || harmful);
  if (malware)
    return SB_REASON_MALWARE;
  else if (harmful)
    return SB_REASON_HARMFUL;
  return SB_REASON_PHISHING;
}

// static
std::unique_ptr<ChromeMetricsHelper>
SafeBrowsingBlockingPage::CreateMetricsHelper(
    WebContents* web_contents,
    const UnsafeResourceList& unsafe_resources) {
  SBInterstitialReason interstitial_reason =
      GetInterstitialReason(unsafe_resources);
  GURL request_url(unsafe_resources[0].url);
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix =
      GetMetricPrefix(unsafe_resources, interstitial_reason);
  reporting_info.extra_suffix = GetExtraMetricsSuffix(unsafe_resources);
  reporting_info.rappor_prefix = GetRapporPrefix(interstitial_reason);
  reporting_info.deprecated_rappor_prefix =
      GetDeprecatedRapporPrefix(interstitial_reason);
  reporting_info.rappor_report_type =
      rappor::LOW_FREQUENCY_SAFEBROWSING_RAPPOR_TYPE;
  reporting_info.deprecated_rappor_report_type =
      rappor::SAFEBROWSING_RAPPOR_TYPE;
  return std::unique_ptr<ChromeMetricsHelper>(
      new ChromeMetricsHelper(web_contents, request_url, reporting_info,
                              GetSamplingEventName(interstitial_reason)));
}

void SafeBrowsingBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  CHECK(load_time_data);
  CHECK(!unsafe_resources_.empty());

  load_time_data->SetString("type", "SAFEBROWSING");
  load_time_data->SetString(
      "tabTitle", l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_TITLE));
  load_time_data->SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_OPEN_DETAILS_BUTTON));
  load_time_data->SetString(
      "closeDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_CLOSE_DETAILS_BUTTON));
  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_OVERRIDABLE_SAFETY_BUTTON));
  load_time_data->SetBoolean(
      "overridable",
      !IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled));
  security_interstitials::common_string_util::PopulateNewIconStrings(
      load_time_data);

  switch (interstitial_reason_) {
    case SB_REASON_MALWARE:
      PopulateMalwareLoadTimeData(load_time_data);
      break;
    case SB_REASON_HARMFUL:
      PopulateHarmfulLoadTimeData(load_time_data);
      break;
    case SB_REASON_PHISHING:
      PopulatePhishingLoadTimeData(load_time_data);
      break;
  }
}

void SafeBrowsingBlockingPage::PopulateExtendedReportingOption(
    base::DictionaryValue* load_time_data) {
  // Only show checkbox if !(HTTPS || incognito-mode).
  const bool show = CanShowThreatDetailsOption();
  load_time_data->SetBoolean(security_interstitials::kDisplayCheckBox, show);
  if (!show)
    return;

  const std::string privacy_link = base::StringPrintf(
      security_interstitials::kPrivacyLinkHtml,
      security_interstitials::CMD_OPEN_REPORTING_PRIVACY,
      l10n_util::GetStringUTF8(IDS_SAFE_BROWSING_PRIVACY_POLICY_PAGE).c_str());
  load_time_data->SetString(
      security_interstitials::kOptInLink,
      l10n_util::GetStringFUTF16(
          ChooseOptInTextResource(*profile()->GetPrefs(),
                                  IDS_SAFE_BROWSING_MALWARE_REPORTING_AGREE,
                                  IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE),
          base::UTF8ToUTF16(privacy_link)));
  load_time_data->SetBoolean(
      security_interstitials::kBoxChecked,
      IsExtendedReportingEnabled(*profile()->GetPrefs()));
}

void SafeBrowsingBlockingPage::PopulateMalwareLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_MALWARE_V3_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_MALWARE_V3_PRIMARY_PARAGRAPH,
          GetFormattedHostName()));
  load_time_data->SetString(
      "explanationParagraph",
      is_main_frame_load_blocked_ ?
          l10n_util::GetStringFUTF16(
              IDS_MALWARE_V3_EXPLANATION_PARAGRAPH,
              GetFormattedHostName()) :
          l10n_util::GetStringFUTF16(
              IDS_MALWARE_V3_EXPLANATION_PARAGRAPH_SUBRESOURCE,
              base::UTF8ToUTF16(main_frame_url_.host()),
              GetFormattedHostName()));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_MALWARE_V3_PROCEED_PARAGRAPH));

  PopulateExtendedReportingOption(load_time_data);
}

void SafeBrowsingBlockingPage::PopulateHarmfulLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_HARMFUL_V3_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_HARMFUL_V3_PRIMARY_PARAGRAPH,
          GetFormattedHostName()));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(
          IDS_HARMFUL_V3_EXPLANATION_PARAGRAPH,
          GetFormattedHostName()));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_HARMFUL_V3_PROCEED_PARAGRAPH));

  PopulateExtendedReportingOption(load_time_data);
}

void SafeBrowsingBlockingPage::PopulatePhishingLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", true);
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_PHISHING_V4_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(IDS_PHISHING_V4_PRIMARY_PARAGRAPH,
                                 GetFormattedHostName()));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(IDS_PHISHING_V4_EXPLANATION_PARAGRAPH,
                                 GetFormattedHostName()));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_PHISHING_V4_PROCEED_AND_REPORT_PARAGRAPH));

  PopulateExtendedReportingOption(load_time_data);
}

}  // namespace safe_browsing
