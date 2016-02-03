// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingBlockingPage class.

#include "ios/chrome/browser/safe_browsing/safe_browsing_blocking_page.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/browser/google_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/interstitials/ios_chrome_controller_client.h"
#include "ios/chrome/browser/interstitials/ios_chrome_metrics_helper.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/safe_browsing/ui_manager.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/interstitials/web_interstitial.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

namespace {

// For malware interstitial pages, we link the problematic URL to Google's
// diagnostic page.
#if defined(GOOGLE_CHROME_BUILD)
const char kSbDiagnosticUrl[] =
    "https://www.google.com/safebrowsing/"
    "diagnostic?site=%s&client=googlechrome";
#else
const char kSbDiagnosticUrl[] =
    "https://www.google.com/safebrowsing/diagnostic?site=%s&client=chromium";
#endif

// URL for malware and phishing, V2.
const char kLearnMoreMalwareUrlV2[] =
    "https://www.google.com/transparencyreport/safebrowsing/";
const char kLearnMorePhishingUrlV2[] =
    "https://www.google.com/transparencyreport/safebrowsing/";

// Constants for the V4 phishing string upgrades.
const char kSocialEngineeringTrial[] = "SafeBrowsingSocialEngineeringStrings";
const char kSocialEngineeringEnabled[] = "Enabled";

// Constants for the Experience Sampling instrumentation.
const char kEventNameMalware[] = "safebrowsing_interstitial_";
const char kEventNameHarmful[] = "harmful_interstitial_";
const char kEventNamePhishing[] = "phishing_interstitial_";
const char kEventNameOther[] = "safebrowsing_other_interstitial_";

// Constants for the V4 phishing string upgrades.
const char kReportPhishingErrorUrl[] =
    "https://www.google.com/safebrowsing/report_error/";
const char kReportPhishingErrorTrial[] = "SafeBrowsingReportPhishingErrorLink";
const char kReportPhishingErrorEnabled[] = "Enabled";

base::LazyInstance<SafeBrowsingBlockingPage::UnsafeResourceMap>
    g_unsafe_resource_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
SafeBrowsingBlockingPageFactory* SafeBrowsingBlockingPage::factory_ = nullptr;

// The default SafeBrowsingBlockingPageFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingBlockingPageFactoryImpl
    : public SafeBrowsingBlockingPageFactory {
 public:
  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* ui_manager,
      web::WebState* web_state,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    return new SafeBrowsingBlockingPage(ui_manager, web_state,
                                        unsafe_resources);
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<
      SafeBrowsingBlockingPageFactoryImpl>;

  SafeBrowsingBlockingPageFactoryImpl() {}

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageFactoryImpl);
};

static base::LazyInstance<SafeBrowsingBlockingPageFactoryImpl>
    g_safe_browsing_blocking_page_factory_impl = LAZY_INSTANCE_INITIALIZER;

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    web::WebState* web_state,
    const UnsafeResourceList& unsafe_resources)
    : IOSSecurityInterstitialPage(web_state, unsafe_resources[0].url),
      ui_manager_(ui_manager),
      is_main_frame_load_blocked_(IsMainPageLoadBlocked(unsafe_resources)),
      unsafe_resources_(unsafe_resources),
      proceeded_(false),
      controller_(new IOSChromeControllerClient(web_state)) {
  bool malware = false;
  bool harmful = false;
  bool phishing = false;
  for (UnsafeResourceList::const_iterator iter = unsafe_resources_.begin();
       iter != unsafe_resources_.end(); ++iter) {
    const UnsafeResource& resource = *iter;
    SBThreatType threat_type = resource.threat_type;
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
    interstitial_reason_ = SB_REASON_MALWARE;
  else if (harmful)
    interstitial_reason_ = SB_REASON_HARMFUL;
  else
    interstitial_reason_ = SB_REASON_PHISHING;

  // This must be done after calculating |interstitial_reason_| above.
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = GetMetricPrefix();
  reporting_info.extra_suffix = GetExtraMetricsSuffix();
  reporting_info.rappor_prefix = GetRapporPrefix();
  reporting_info.rappor_report_type = rappor::SAFEBROWSING_RAPPOR_TYPE;
  controller_->set_metrics_helper(make_scoped_ptr(
      new IOSChromeMetricsHelper(web_state, request_url(), reporting_info)));
  controller_->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::SHOW);
  controller_->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);
  if (IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled)) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEEDING_DISABLED);
  }

  if (!is_main_frame_load_blocked_) {
    navigation_entry_index_to_remove_ =
        web_state->GetNavigationManager()->GetLastCommittedItemIndex();
  } else {
    navigation_entry_index_to_remove_ = -1;
  }
}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {}

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
      controller_->SetReportingPreference(true);
      break;
    }
    case security_interstitials::CMD_DONT_REPORT: {
      // User disabled SB Extended Reporting via the checkbox.
      controller_->SetReportingPreference(false);
      break;
    }
    case security_interstitials::CMD_OPEN_HELP_CENTER: {
      // User pressed "Learn more".
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
      GURL learn_more_url(interstitial_reason_ == SB_REASON_PHISHING
                              ? kLearnMorePhishingUrlV2
                              : kLearnMoreMalwareUrlV2);
      learn_more_url = google_util::AppendGoogleLocaleParam(
          learn_more_url, GetApplicationContext()->GetApplicationLocale());
      web_state()->OpenURL(web::WebState::OpenURLParams(
          learn_more_url, web::Referrer(), CURRENT_TAB,
          ui::PAGE_TRANSITION_LINK, false));
      break;
    }
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY: {
      // User pressed on the SB Extended Reporting "privacy policy" link.
      controller_->OpenExtendedReportingPrivacyPolicy();
      break;
    }
    case security_interstitials::CMD_PROCEED: {
      // User pressed on the button to proceed.
      if (!IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled)) {
        controller_->metrics_helper()->RecordUserDecision(
            security_interstitials::MetricsHelper::PROCEED);
        web_interstitial()->Proceed();
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
        web_interstitial()->DontProceed();
        // |this| has been deleted after DontProceed() returns.
        break;
      }

      // Otherwise the offending entry has committed, and we need to go back or
      // to a safe page.  We will close the interstitial when that page commits.
      if (web_state()->GetNavigationManager()->CanGoBack()) {
        web_state()->GetNavigationManager()->GoBack();
      } else {
        web_state()->OpenURL(web::WebState::OpenURLParams(
            GURL(kChromeUINewTabURL), web::Referrer(), CURRENT_TAB,
            ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
      }
      break;
    }
    case security_interstitials::CMD_OPEN_DIAGNOSTIC: {
      // User wants to see why this page is blocked.
      const UnsafeResource& unsafe_resource = unsafe_resources_[0];
      std::string bad_url_spec = unsafe_resource.url.spec();
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_DIAGNOSTIC);
      std::string diagnostic = base::StringPrintf(
          kSbDiagnosticUrl,
          net::EscapeQueryParamValue(bad_url_spec, true).c_str());
      GURL diagnostic_url(diagnostic);
      diagnostic_url = google_util::AppendGoogleLocaleParam(
          diagnostic_url, GetApplicationContext()->GetApplicationLocale());
      DCHECK(unsafe_resource.threat_type == SB_THREAT_TYPE_URL_MALWARE ||
             unsafe_resource.threat_type ==
                 SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL ||
             unsafe_resource.threat_type == SB_THREAT_TYPE_URL_UNWANTED);
      web_state()->OpenURL(web::WebState::OpenURLParams(
          diagnostic_url, web::Referrer(), CURRENT_TAB,
          ui::PAGE_TRANSITION_LINK, false));
      break;
    }
    case security_interstitials::CMD_SHOW_MORE_SECTION: {
      // User has opened up the hidden text.
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    }
    case security_interstitials::CMD_REPORT_PHISHING_ERROR: {
      // User wants to report a phishing error.
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::REPORT_PHISHING_ERROR);
      GURL phishing_error_url(kReportPhishingErrorUrl);
      phishing_error_url = google_util::AppendGoogleLocaleParam(
          phishing_error_url, GetApplicationContext()->GetApplicationLocale());
      web_state()->OpenURL(web::WebState::OpenURLParams(
          phishing_error_url, web::Referrer(), CURRENT_TAB,
          ui::PAGE_TRANSITION_LINK, false));
      break;
    }
  }
}

void SafeBrowsingBlockingPage::OnProceed() {
  proceeded_ = true;
  ui_manager_->OnBlockingPageDone(unsafe_resources_, true);

  // Check to see if some new notifications of unsafe resources have been
  // received while we were showing the interstitial.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  UnsafeResourceMap::iterator iter = unsafe_resource_map->find(web_state());
  SafeBrowsingBlockingPage* blocking_page = nullptr;
  if (iter != unsafe_resource_map->end() && !iter->second.empty()) {
    // Build an interstitial for all the unsafe resources notifications.
    // Don't show it now as showing an interstitial while an interstitial is
    // already showing would cause DontProceed() to be invoked.
    blocking_page = factory_->CreateSafeBrowsingPage(ui_manager_, web_state(),
                                                     iter->second);
    unsafe_resource_map->erase(iter);
  }

  // Now that this interstitial is gone, we can show the new one.
  if (blocking_page)
    blocking_page->Show();
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
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
  }

  ui_manager_->OnBlockingPageDone(unsafe_resources_, false);

  // The user does not want to proceed, clear the queued unsafe resources
  // notifications we received while the interstitial was showing.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  UnsafeResourceMap::iterator iter = unsafe_resource_map->find(web_state());
  if (iter != unsafe_resource_map->end() && !iter->second.empty()) {
    ui_manager_->OnBlockingPageDone(iter->second, false);
    unsafe_resource_map->erase(iter);
  }

  // We don't remove the navigation entry if the tab is being destroyed as this
  // would trigger a navigation that would cause trouble as the render view host
  // for the tab has by then already been destroyed.  We also don't delete the
  // current entry if it has been committed again, which is possible on a page
  // that had a subresource warning.
  int last_committed_index =
      web_state()->GetNavigationManager()->GetLastCommittedItemIndex();
  if (navigation_entry_index_to_remove_ != -1 &&
      navigation_entry_index_to_remove_ != last_committed_index &&
      !web_state()->IsBeingDestroyed()) {
    CHECK(web_state()->GetNavigationManager()->RemoveItemAtIndex(
        navigation_entry_index_to_remove_));
    navigation_entry_index_to_remove_ = -1;
  }
}

// static
SafeBrowsingBlockingPage::UnsafeResourceMap*
SafeBrowsingBlockingPage::GetUnsafeResourcesMap() {
  return g_unsafe_resource_map.Pointer();
}

// static
SafeBrowsingBlockingPage* SafeBrowsingBlockingPage::CreateBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    web::WebState* web_state,
    const UnsafeResource& unsafe_resource) {
  std::vector<UnsafeResource> resources;
  resources.push_back(unsafe_resource);
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_safe_browsing_blocking_page_factory_impl.Pointer();
  return factory_->CreateSafeBrowsingPage(ui_manager, web_state, resources);
}

// static
void SafeBrowsingBlockingPage::ShowBlockingPage(
    web::WebState* web_state,
    SafeBrowsingUIManager* ui_manager,
    const UnsafeResource& unsafe_resource) {
  DVLOG(1) << __FUNCTION__ << " " << unsafe_resource.url.spec();
  web::WebInterstitial* web_interstitial =
      web::WebInterstitial::GetWebInterstitial(web_state);
  if (web_interstitial && !unsafe_resource.is_subresource) {
    // There is already an interstitial showing and we are about to display a
    // new one for the main frame. Just hide the current one, it is now
    // irrelevent
    web_interstitial->DontProceed();
    web_interstitial = nullptr;
  }

  if (!web_interstitial) {
    // There are no interstitial currently showing in that tab, go ahead and
    // show this interstitial.
    SafeBrowsingBlockingPage* blocking_page =
        CreateBlockingPage(ui_manager, web_state, unsafe_resource);
    blocking_page->Show();
    return;
  }

  // This is an interstitial for a page's resource, let's queue it.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  (*unsafe_resource_map)[web_state].push_back(unsafe_resource);
}

// static
bool SafeBrowsingBlockingPage::IsMainPageLoadBlocked(
    const UnsafeResourceList& unsafe_resources) {
  // If there is more than one unsafe resource, the main page load must not be
  // blocked. Otherwise, check if the one resource is.
  return unsafe_resources.size() == 1 &&
         unsafe_resources[0].IsMainPageLoadBlocked();
}

std::string SafeBrowsingBlockingPage::GetMetricPrefix() const {
  bool primary_subresource = unsafe_resources_[0].is_subresource;
  switch (interstitial_reason_) {
    case SB_REASON_MALWARE:
      return primary_subresource ? "malware_subresource" : "malware";
    case SB_REASON_HARMFUL:
      return primary_subresource ? "harmful_subresource" : "harmful";
    case SB_REASON_PHISHING:
      return primary_subresource ? "phishing_subresource" : "phishing";
  }
  NOTREACHED();
  return std::string();
}

// We populate a parallel set of metrics to differentiate some threat sources.
std::string SafeBrowsingBlockingPage::GetExtraMetricsSuffix() const {
  switch (unsafe_resources_[0].threat_source) {
    case safe_browsing::ThreatSource::DATA_SAVER:
      return "from_data_saver";
    case safe_browsing::ThreatSource::REMOTE:
    case safe_browsing::ThreatSource::LOCAL_PVER3:
      // REMOTE and LOCAL_PVER3 can be distinguished in the logs
      // by platform type: Remote is mobile, local_pver3 is desktop.
      return "from_device";
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      return "from_device_v4";
    case safe_browsing::ThreatSource::UNKNOWN:
      break;
  }
  NOTREACHED();
  return std::string();
}

std::string SafeBrowsingBlockingPage::GetRapporPrefix() const {
  switch (interstitial_reason_) {
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

std::string SafeBrowsingBlockingPage::GetSamplingEventName() const {
  switch (interstitial_reason_) {
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

void SafeBrowsingBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) const {
  CHECK(load_time_data);
  CHECK(!unsafe_resources_.empty());

  load_time_data->SetString("type", "SAFEBROWSING");
  load_time_data->SetString(
      "tabTitle", l10n_util::GetStringUTF16(IDS_IOS_SAFEBROWSING_V3_TITLE));
  load_time_data->SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_IOS_SAFEBROWSING_V3_OPEN_DETAILS_BUTTON));
  load_time_data->SetString(
      "closeDetails",
      l10n_util::GetStringUTF16(IDS_IOS_SAFEBROWSING_V3_CLOSE_DETAILS_BUTTON));
  load_time_data->SetString(
      "primaryButtonText", l10n_util::GetStringUTF16(
                               IDS_IOS_SAFEBROWSING_OVERRIDABLE_SAFETY_BUTTON));
  // TODO(crbug.com/390675): Undo this forkage. This is a temporary fix to make
  // sure the broken proceed-from-unsafe-resource path can't be hit on iOS.
  // Always set subresource alerts to be non-overridable. Otherwise obey the
  // global pref.
  bool overridable = !unsafe_resources_[0].is_subresource &&
                     !IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled);
  load_time_data->SetBoolean("overridable", overridable);

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

void SafeBrowsingBlockingPage::AfterShow() {
  controller_->SetWebInterstitial(web_interstitial());
}

void SafeBrowsingBlockingPage::PopulateMalwareLoadTimeData(
    base::DictionaryValue* load_time_data) const {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_IOS_MALWARE_V3_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(IDS_IOS_MALWARE_V3_PRIMARY_PARAGRAPH,
                                 GetFormattedHostName()));
  load_time_data->SetString(
      "explanationParagraph",
      is_main_frame_load_blocked_
          ? l10n_util::GetStringFUTF16(IDS_IOS_MALWARE_V3_EXPLANATION_PARAGRAPH,
                                       GetFormattedHostName())
          : l10n_util::GetStringFUTF16(
                IDS_IOS_MALWARE_V3_EXPLANATION_PARAGRAPH_SUBRESOURCE,
                base::UTF8ToUTF16(web_state()->GetVisibleURL().host()),
                GetFormattedHostName()));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_IOS_MALWARE_V3_PROCEED_PARAGRAPH));

  load_time_data->SetBoolean(security_interstitials::kDisplayCheckBox, false);
}

void SafeBrowsingBlockingPage::PopulateHarmfulLoadTimeData(
    base::DictionaryValue* load_time_data) const {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_IOS_HARMFUL_V3_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(IDS_IOS_HARMFUL_V3_PRIMARY_PARAGRAPH,
                                 GetFormattedHostName()));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(IDS_IOS_HARMFUL_V3_EXPLANATION_PARAGRAPH,
                                 GetFormattedHostName()));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_IOS_HARMFUL_V3_PROCEED_PARAGRAPH));

  load_time_data->SetBoolean(security_interstitials::kDisplayCheckBox, false);
}

void SafeBrowsingBlockingPage::PopulatePhishingLoadTimeData(
    base::DictionaryValue* load_time_data) const {
  bool use_social_engineering_strings =
      base::FieldTrialList::FindFullName(kSocialEngineeringTrial) ==
      kSocialEngineeringEnabled;
  load_time_data->SetBoolean("phishing", true);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(use_social_engineering_strings
                                               ? IDS_IOS_PHISHING_V4_HEADING
                                               : IDS_IOS_PHISHING_V3_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(use_social_engineering_strings
                                     ? IDS_IOS_PHISHING_V4_PRIMARY_PARAGRAPH
                                     : IDS_IOS_PHISHING_V3_PRIMARY_PARAGRAPH,
                                 GetFormattedHostName()));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(IDS_IOS_PHISHING_V3_EXPLANATION_PARAGRAPH,
                                 GetFormattedHostName()));

  if (base::FieldTrialList::FindFullName(kReportPhishingErrorTrial) ==
      kReportPhishingErrorEnabled) {
    load_time_data->SetString(
        "finalParagraph",
        l10n_util::GetStringUTF16(
            IDS_IOS_PHISHING_V4_PROCEED_AND_REPORT_PARAGRAPH));
  } else {
    load_time_data->SetString(
        "finalParagraph",
        l10n_util::GetStringUTF16(IDS_IOS_PHISHING_V3_PROCEED_PARAGRAPH));
  }

  load_time_data->SetBoolean(security_interstitials::kDisplayCheckBox, false);
}

}  // namespace safe_browsing
