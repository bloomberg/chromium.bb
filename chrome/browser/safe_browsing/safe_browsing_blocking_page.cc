// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingBlockingPage class.

#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"

#include <string>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"

using content::BrowserThread;
using content::InterstitialPage;
using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;
using content::WebContents;

// For malware interstitial pages, we link the problematic URL to Google's
// diagnostic page.
#if defined(GOOGLE_CHROME_BUILD)
static const char* const kSbDiagnosticUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?site=%s&client=googlechrome";
#else
static const char* const kSbDiagnosticUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?site=%s&client=chromium";
#endif

static const char* const kSbReportPhishingErrorUrl =
    "http://www.google.com/safebrowsing/report_error/";

// URL for the "Learn more" link on the multi threat malware blocking page.
static const char* const kLearnMoreMalwareUrl =
    "https://www.google.com/support/bin/answer.py?answer=45449&topic=360"
    "&sa=X&oi=malwarewarninglink&resnum=1&ct=help";
static const char* const kLearnMoreMalwareUrlV2 =
    "https://www.google.com/goodtoknow/online-safety/malware/";
static const char* const kLearnMorePhishingUrlV2 =
    "https://www.google.com/goodtoknow/online-safety/phishing/";

// URL for the "Learn more" link on the phishing blocking page.
static const char* const kLearnMorePhishingUrl =
    "https://www.google.com/support/bin/answer.py?answer=106318";

static const char* const kSbDiagnosticHtml =
    "<a href=\"\" onclick=\"sendCommand('showDiagnostic'); return false;\" "
    "onmousedown=\"return false;\">%s</a>";

static const char* const kPLinkHtml =
    "<a href=\"\" onclick=\"sendCommand('proceed'); return false;\" "
    "onmousedown=\"return false;\">%s</a>";

static const char* const kPrivacyLinkHtml =
    "<a id=\"privacy-link\" href=\"\" onclick=\"sendCommand('showPrivacy'); "
    "return false;\" onmousedown=\"return false;\">%s</a>";

// After a malware interstitial where the user opted-in to the report
// but clicked "proceed anyway", we delay the call to
// MalwareDetails::FinishCollection() by this much time (in
// milliseconds).
static const int64 kMalwareDetailsProceedDelayMilliSeconds = 3000;

// The commands returned by the page when the user performs an action.
static const char* const kShowDiagnosticCommand = "showDiagnostic";
static const char* const kReportErrorCommand = "reportError";
static const char* const kLearnMoreCommand = "learnMore";
static const char* const kLearnMoreCommandV2 = "learnMore2";
static const char* const kShowPrivacyCommand = "showPrivacy";
static const char* const kProceedCommand = "proceed";
static const char* const kTakeMeBackCommand = "takeMeBack";
static const char* const kDoReportCommand = "doReport";
static const char* const kDontReportCommand = "dontReport";
static const char* const kDisplayCheckBox = "displaycheckbox";
static const char* const kBoxChecked = "boxchecked";
static const char* const kExpandedSeeMore = "expandedSeeMore";
// Special command that we use when the user navigated away from the
// page.  E.g., closed the tab or the window.  This is only used by
// RecordUserReactionTime.
static const char* const kNavigatedAwayMetaCommand = "closed";

// static
SafeBrowsingBlockingPageFactory* SafeBrowsingBlockingPage::factory_ = NULL;

static base::LazyInstance<SafeBrowsingBlockingPage::UnsafeResourceMap>
    g_unsafe_resource_map = LAZY_INSTANCE_INITIALIZER;

// The default SafeBrowsingBlockingPageFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingBlockingPageFactoryImpl
    : public SafeBrowsingBlockingPageFactory {
 public:
  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) {
    // Only do the trial if the interstitial is for a single malware or
    // phishing resource, the multi-threat interstitial has not been updated to
    // V2 yet.
    if (unsafe_resources.size() == 1 &&
        (unsafe_resources[0].threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         unsafe_resources[0].threat_type == SB_THREAT_TYPE_URL_PHISHING)) {
      return new SafeBrowsingBlockingPageV2(ui_manager, web_contents,
          unsafe_resources);
    }
    return new SafeBrowsingBlockingPageV1(ui_manager, web_contents,
                                          unsafe_resources);
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<
      SafeBrowsingBlockingPageFactoryImpl>;

  SafeBrowsingBlockingPageFactoryImpl() { }

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageFactoryImpl);
};

static base::LazyInstance<SafeBrowsingBlockingPageFactoryImpl>
    g_safe_browsing_blocking_page_factory_impl = LAZY_INSTANCE_INITIALIZER;

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    WebContents* web_contents,
    const UnsafeResourceList& unsafe_resources)
    : malware_details_proceed_delay_ms_(
          kMalwareDetailsProceedDelayMilliSeconds),
      ui_manager_(ui_manager),
      report_loop_(NULL),
      is_main_frame_load_blocked_(IsMainPageLoadBlocked(unsafe_resources)),
      unsafe_resources_(unsafe_resources),
      proceeded_(false),
      web_contents_(web_contents),
      url_(unsafe_resources[0].url),
      has_expanded_see_more_section_(false) {
  bool malware = false;
  bool phishing = false;
  for (UnsafeResourceList::const_iterator iter = unsafe_resources_.begin();
       iter != unsafe_resources_.end(); ++iter) {
    const UnsafeResource& resource = *iter;
    SBThreatType threat_type = resource.threat_type;
    if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
      malware = true;
    } else {
      DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING ||
             threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL);
      phishing = true;
    }
  }
  DCHECK(phishing || malware);
  if (malware && phishing)
    interstitial_type_ = TYPE_MALWARE_AND_PHISHING;
  else if (malware)
    interstitial_type_ = TYPE_MALWARE;
  else
    interstitial_type_ = TYPE_PHISHING;
  RecordUserAction(SHOW);
  if (!is_main_frame_load_blocked_) {
    navigation_entry_index_to_remove_ =
        web_contents->GetController().GetLastCommittedEntryIndex();
  } else {
    navigation_entry_index_to_remove_ = -1;
  }

  // Start computing malware details. They will be sent only
  // if the user opts-in on the blocking page later.
  // If there's more than one malicious resources, it means the user
  // clicked through the first warning, so we don't prepare additional
  // reports.
  if (unsafe_resources.size() == 1 &&
      unsafe_resources[0].threat_type == SB_THREAT_TYPE_URL_MALWARE &&
      malware_details_ == NULL &&
      CanShowMalwareDetailsOption()) {
    malware_details_ = MalwareDetails::NewMalwareDetails(
        ui_manager_, web_contents, unsafe_resources[0]);
  }

  interstitial_page_ = InterstitialPage::Create(
      web_contents, IsMainPageLoadBlocked(unsafe_resources), url_, this);
}

bool SafeBrowsingBlockingPage::CanShowMalwareDetailsOption() {
  return (!web_contents_->GetBrowserContext()->IsOffTheRecord() &&
          web_contents_->GetURL().SchemeIs(chrome::kHttpScheme));
}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {
}

void SafeBrowsingBlockingPage::CommandReceived(const std::string& cmd) {
  std::string command(cmd);  // Make a local copy so we can modify it.
  // The Jasonified response has quotes, remove them.
  if (command.length() > 1 && command[0] == '"') {
    command = command.substr(1, command.length() - 2);
  }
  RecordUserReactionTime(command);
  if (command == kDoReportCommand) {
    SetReportingPreference(true);
    return;
  }

  if (command == kDontReportCommand) {
    SetReportingPreference(false);
    return;
  }

  if (command == kLearnMoreCommand) {
    // User pressed "Learn more".
    GURL url;
    SBThreatType threat_type = unsafe_resources_[0].threat_type;
    if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
      url = google_util::AppendGoogleLocaleParam(GURL(kLearnMoreMalwareUrl));
    } else if (threat_type == SB_THREAT_TYPE_URL_PHISHING ||
               threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL) {
      url = google_util::AppendGoogleLocaleParam(GURL(kLearnMorePhishingUrl));
    } else {
      NOTREACHED();
    }

    OpenURLParams params(
        url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params);
    return;
  }

  if (command == kLearnMoreCommandV2) {
    // User pressed "Learn more".
    GURL url;
    SBThreatType threat_type = unsafe_resources_[0].threat_type;
    if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
      url = google_util::AppendGoogleLocaleParam(GURL(kLearnMoreMalwareUrlV2));
    } else if (threat_type == SB_THREAT_TYPE_URL_PHISHING ||
               threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL) {
      url = google_util::AppendGoogleLocaleParam(GURL(kLearnMorePhishingUrlV2));
    } else {
      NOTREACHED();
    }

    OpenURLParams params(
        url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params);
    return;
  }

  if (command == kShowPrivacyCommand) {
    // User pressed "Safe Browsing privacy policy".
    GURL url(l10n_util::GetStringUTF8(IDS_SAFE_BROWSING_PRIVACY_POLICY_URL));
    OpenURLParams params(
        url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params);
    return;
  }

  bool proceed_blocked = false;
  if (command == kProceedCommand) {
    if (IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled)) {
      proceed_blocked = true;
    } else {
      interstitial_page_->Proceed();
      // |this| has been deleted after Proceed() returns.
      return;
    }
  }

  if (command == kTakeMeBackCommand || proceed_blocked) {
    if (is_main_frame_load_blocked_) {
      // If the load is blocked, we want to close the interstitial and discard
      // the pending entry.
      interstitial_page_->DontProceed();
      // |this| has been deleted after DontProceed() returns.
      return;
    }

    // Otherwise the offending entry has committed, and we need to go back or
    // to a safe page.  We will close the interstitial when that page commits.
    if (web_contents_->GetController().CanGoBack()) {
      web_contents_->GetController().GoBack();
    } else {
      web_contents_->GetController().LoadURL(
          GURL(chrome::kChromeUINewTabURL),
          content::Referrer(),
          content::PAGE_TRANSITION_AUTO_TOPLEVEL,
          std::string());
    }
    return;
  }

  // The "report error" and "show diagnostic" commands can have a number
  // appended to them, which is the index of the element they apply to.
  size_t element_index = 0;
  size_t colon_index = command.find(':');
  if (colon_index != std::string::npos) {
    DCHECK(colon_index < command.size() - 1);
    int result_int = 0;
    bool result = base::StringToInt(base::StringPiece(command.begin() +
                                                      colon_index + 1,
                                                      command.end()),
                                    &result_int);
    command = command.substr(0, colon_index);
    if (result)
      element_index = static_cast<size_t>(result_int);
  }

  if (element_index >= unsafe_resources_.size()) {
    NOTREACHED();
    return;
  }

  std::string bad_url_spec = unsafe_resources_[element_index].url.spec();
  if (command == kReportErrorCommand) {
    // User pressed "Report error" for a phishing site.
    // Note that we cannot just put a link in the interstitial at this point.
    // It is not OK to navigate in the context of an interstitial page.
    SBThreatType threat_type = unsafe_resources_[element_index].threat_type;
    DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING ||
           threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL);
    GURL report_url =
        safe_browsing_util::GeneratePhishingReportUrl(
            kSbReportPhishingErrorUrl,
            bad_url_spec,
            threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL);
    OpenURLParams params(
        report_url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK,
        false);
    web_contents_->OpenURL(params);
    return;
  }

  if (command == kShowDiagnosticCommand) {
    // We're going to take the user to Google's SafeBrowsing diagnostic page.
    std::string diagnostic =
        base::StringPrintf(kSbDiagnosticUrl,
            net::EscapeQueryParamValue(bad_url_spec, true).c_str());
    GURL diagnostic_url(diagnostic);
    diagnostic_url = google_util::AppendGoogleLocaleParam(diagnostic_url);
    DCHECK(unsafe_resources_[element_index].threat_type ==
           SB_THREAT_TYPE_URL_MALWARE);
    OpenURLParams params(
        diagnostic_url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK,
        false);
    web_contents_->OpenURL(params);
    return;
  }

  if (command == kExpandedSeeMore) {
    // User expanded the "see more info" section of the page.  We don't actually
    // do any action based on this, it's just so that RecordUserReactionTime can
    // track it.
    return;
  }

  NOTREACHED() << "Unexpected command: " << command;
}

void SafeBrowsingBlockingPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
 }

void SafeBrowsingBlockingPage::SetReportingPreference(bool report) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  PrefService* pref = profile->GetPrefs();
  pref->SetBoolean(prefs::kSafeBrowsingReportingEnabled, report);
}

void SafeBrowsingBlockingPage::OnProceed() {
  proceeded_ = true;
  RecordUserAction(PROCEED);
  // Send the malware details, if we opted to.
  FinishMalwareDetails(malware_details_proceed_delay_ms_);

  NotifySafeBrowsingUIManager(ui_manager_, unsafe_resources_, true);

  // Check to see if some new notifications of unsafe resources have been
  // received while we were showing the interstitial.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  UnsafeResourceMap::iterator iter = unsafe_resource_map->find(web_contents_);
  SafeBrowsingBlockingPage* blocking_page = NULL;
  if (iter != unsafe_resource_map->end() && !iter->second.empty()) {
    // Build an interstitial for all the unsafe resources notifications.
    // Don't show it now as showing an interstitial while an interstitial is
    // already showing would cause DontProceed() to be invoked.
    blocking_page = factory_->CreateSafeBrowsingPage(ui_manager_, web_contents_,
                                                     iter->second);
    unsafe_resource_map->erase(iter);
  }

  // Now that this interstitial is gone, we can show the new one.
  if (blocking_page)
    blocking_page->interstitial_page_->Show();
}

void SafeBrowsingBlockingPage::OnDontProceed() {
  // Calling this method twice will not double-count.
  RecordUserReactionTime(kNavigatedAwayMetaCommand);
  // We could have already called Proceed(), in which case we must not notify
  // the SafeBrowsingUIManager again, as the client has been deleted.
  if (proceeded_)
    return;

  RecordUserAction(DONT_PROCEED);
  // Send the malware details, if we opted to.
  FinishMalwareDetails(0);  // No delay

  NotifySafeBrowsingUIManager(ui_manager_, unsafe_resources_, false);

  // The user does not want to proceed, clear the queued unsafe resources
  // notifications we received while the interstitial was showing.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  UnsafeResourceMap::iterator iter = unsafe_resource_map->find(web_contents_);
  if (iter != unsafe_resource_map->end() && !iter->second.empty()) {
    NotifySafeBrowsingUIManager(ui_manager_, iter->second, false);
    unsafe_resource_map->erase(iter);
  }

  // We don't remove the navigation entry if the tab is being destroyed as this
  // would trigger a navigation that would cause trouble as the render view host
  // for the tab has by then already been destroyed.  We also don't delete the
  // current entry if it has been committed again, which is possible on a page
  // that had a subresource warning.
  int last_committed_index =
      web_contents_->GetController().GetLastCommittedEntryIndex();
  if (navigation_entry_index_to_remove_ != -1 &&
      navigation_entry_index_to_remove_ != last_committed_index &&
      !web_contents_->IsBeingDestroyed()) {
    web_contents_->GetController().RemoveEntryAtIndex(
        navigation_entry_index_to_remove_);
    navigation_entry_index_to_remove_ = -1;
  }
}

void SafeBrowsingBlockingPage::RecordUserAction(BlockingPageEvent event) {
  // This enum is used for a histogram.  Don't reorder, delete, or insert
  // elements.  New elements should be added before MAX_ACTION only.
  enum {
    MALWARE_SHOW = 0,
    MALWARE_DONT_PROCEED,
    MALWARE_FORCED_DONT_PROCEED,
    MALWARE_PROCEED,
    MULTIPLE_SHOW,
    MULTIPLE_DONT_PROCEED,
    MULTIPLE_FORCED_DONT_PROCEED,
    MULTIPLE_PROCEED,
    PHISHING_SHOW,
    PHISHING_DONT_PROCEED,
    PHISHING_FORCED_DONT_PROCEED,
    PHISHING_PROCEED,
    MAX_ACTION
  } histogram_action = MAX_ACTION;

  switch (event) {
    case SHOW:
      switch (interstitial_type_) {
        case TYPE_MALWARE_AND_PHISHING:
          histogram_action = MULTIPLE_SHOW;
          break;
        case TYPE_MALWARE:
          histogram_action = MALWARE_SHOW;
          break;
        case TYPE_PHISHING:
          histogram_action = PHISHING_SHOW;
          break;
      }
      break;
    case PROCEED:
      switch (interstitial_type_) {
        case TYPE_MALWARE_AND_PHISHING:
          histogram_action = MULTIPLE_PROCEED;
          break;
        case TYPE_MALWARE:
          histogram_action = MALWARE_PROCEED;
          break;
        case TYPE_PHISHING:
          histogram_action = PHISHING_PROCEED;
          break;
      }
      break;
    case DONT_PROCEED:
      if (IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled)) {
        switch (interstitial_type_) {
          case TYPE_MALWARE_AND_PHISHING:
            histogram_action = MULTIPLE_FORCED_DONT_PROCEED;
            break;
          case TYPE_MALWARE:
            histogram_action = MALWARE_FORCED_DONT_PROCEED;
            break;
          case TYPE_PHISHING:
            histogram_action = PHISHING_FORCED_DONT_PROCEED;
            break;
        }
      } else {
        switch (interstitial_type_) {
          case TYPE_MALWARE_AND_PHISHING:
            histogram_action = MULTIPLE_DONT_PROCEED;
            break;
          case TYPE_MALWARE:
            histogram_action = MALWARE_DONT_PROCEED;
            break;
          case TYPE_PHISHING:
            histogram_action = PHISHING_DONT_PROCEED;
            break;
        }
      }
      break;
    default:
      NOTREACHED() << "Unexpected event: " << event;
  }
  if (histogram_action == MAX_ACTION) {
    NOTREACHED();
  } else {
    UMA_HISTOGRAM_ENUMERATION("SB2.InterstitialAction", histogram_action,
                              MAX_ACTION);
  }

  // TODO(mattm): now that we've added the histogram above, should we remove
  // this old user metric at some future point?
  std::string action = "SBInterstitial";
  switch (interstitial_type_) {
    case TYPE_MALWARE_AND_PHISHING:
      action.append("Multiple");
      break;
    case TYPE_MALWARE:
      action.append("Malware");
      break;
    case TYPE_PHISHING:
      action.append("Phishing");
      break;
  }

  switch (event) {
    case SHOW:
      action.append("Show");
      break;
    case PROCEED:
      action.append("Proceed");
      break;
    case DONT_PROCEED:
      if (IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled))
        action.append("ForcedDontProceed");
      else
        action.append("DontProceed");
      break;
    default:
      NOTREACHED() << "Unexpected event: " << event;
  }

  content::RecordComputedAction(action);
}

void SafeBrowsingBlockingPage::RecordUserReactionTime(
    const std::string& command) {
  if (interstitial_show_time_.is_null())
    return;  // We already reported the user reaction time.
  base::TimeDelta dt = base::TimeTicks::Now() - interstitial_show_time_;
  DVLOG(1) << "User reaction time for command:" << command
           << " on interstitial_type_:" << interstitial_type_
           << " warning took " << dt.InMilliseconds() << "ms";
  bool recorded = true;
  if (interstitial_type_ == TYPE_MALWARE ||
      interstitial_type_ == TYPE_MALWARE_AND_PHISHING) {
    // There are six ways in which the malware interstitial can go
    // away.  We handle all of them here but we group two together: closing the
    // tag / browser window and clicking on the back button in the browser (not
    // the big green button) are considered the same action.
    if (command == kProceedCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.MalwareInterstitialTimeProceed", dt);
    } else if (command == kTakeMeBackCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.MalwareInterstitialTimeTakeMeBack", dt);
    } else if (command == kShowDiagnosticCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.MalwareInterstitialTimeDiagnostic", dt);
    } else if (command == kShowPrivacyCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.MalwareInterstitialTimePrivacyPolicy",
                                 dt);
    } else if (command == kLearnMoreCommand || command == kLearnMoreCommandV2) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.MalwareInterstitialLearnMore",
                                 dt);
    } else if (command == kNavigatedAwayMetaCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.MalwareInterstitialTimeClosed", dt);
    } else if (command == kExpandedSeeMore) {
      // Only record the expanded histogram once per display of the
      // interstitial.
      if (has_expanded_see_more_section_)
        return;

      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.MalwareInterstitialTimeExpandedSeeMore",
                                 dt);
      has_expanded_see_more_section_ = true;
      // Expanding the "See More" section doesn't finish the interstitial, so
      // don't mark the reaction time as recorded.
      recorded = false;
    } else {
      recorded = false;
    }
  } else {
    // Same as above but for phishing warnings.
    if (command == kProceedCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.PhishingInterstitialTimeProceed", dt);
    } else if (command == kTakeMeBackCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.PhishingInterstitialTimeTakeMeBack", dt);
    } else if (command == kShowDiagnosticCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.PhishingInterstitialTimeReportError", dt);
    } else if (command == kLearnMoreCommand || command == kLearnMoreCommandV2) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.PhishingInterstitialTimeLearnMore", dt);
    } else if (command == kNavigatedAwayMetaCommand) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.PhishingInterstitialTimeClosed", dt);
    } else if (command == kExpandedSeeMore) {
      // Only record the expanded histogram once per display of the
      // interstitial.
      if (has_expanded_see_more_section_)
        return;

      UMA_HISTOGRAM_MEDIUM_TIMES("SB2.PhishingInterstitialTimeExpandedSeeMore",
                                 dt);
      has_expanded_see_more_section_ = true;
      // Expanding the "See More" section doesn't finish the interstitial, so
      // don't mark the reaction time as recorded.
      recorded = false;
    } else {
      recorded = false;
    }
  }
  if (recorded)  // Making sure we don't double-count reaction times.
    interstitial_show_time_ = base::TimeTicks();  //  Resets the show time.
}

void SafeBrowsingBlockingPage::FinishMalwareDetails(int64 delay_ms) {
  if (malware_details_ == NULL)
    return;  // Not all interstitials have malware details (eg phishing).

  if (IsPrefEnabled(prefs::kSafeBrowsingReportingEnabled)) {
    // Finish the malware details collection, send it over.
    BrowserThread::PostDelayedTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MalwareDetails::FinishCollection, malware_details_.get()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }
}

bool SafeBrowsingBlockingPage::IsPrefEnabled(const char* pref) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile->GetPrefs()->GetBoolean(pref);
}

// static
void SafeBrowsingBlockingPage::NotifySafeBrowsingUIManager(
    SafeBrowsingUIManager* ui_manager,
    const UnsafeResourceList& unsafe_resources,
    bool proceed) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingUIManager::OnBlockingPageDone,
                 ui_manager, unsafe_resources, proceed));
}

// static
SafeBrowsingBlockingPage::UnsafeResourceMap*
    SafeBrowsingBlockingPage::GetUnsafeResourcesMap() {
  return g_unsafe_resource_map.Pointer();
}

// static
void SafeBrowsingBlockingPage::ShowBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    const UnsafeResource& unsafe_resource) {
  DVLOG(1) << __FUNCTION__ << " " << unsafe_resource.url.spec();
  WebContents* web_contents = tab_util::GetWebContentsByID(
      unsafe_resource.render_process_host_id, unsafe_resource.render_view_id);

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
    std::vector<UnsafeResource> resources;
    resources.push_back(unsafe_resource);
    // Set up the factory if this has not been done already (tests do that
    // before this method is called).
    if (!factory_)
      factory_ = g_safe_browsing_blocking_page_factory_impl.Pointer();
    SafeBrowsingBlockingPage* blocking_page =
        factory_->CreateSafeBrowsingPage(ui_manager, web_contents, resources);
    blocking_page->interstitial_page_->Show();
    return;
  }

  // This is an interstitial for a page's resource, let's queue it.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  (*unsafe_resource_map)[web_contents].push_back(unsafe_resource);
}

// static
bool SafeBrowsingBlockingPage::IsMainPageLoadBlocked(
    const UnsafeResourceList& unsafe_resources) {
  // Client-side phishing detection interstitials never block the main frame
  // load, since they happen after the page is finished loading.
  if (unsafe_resources[0].threat_type ==
      SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL) {
    return false;
  }

  // Otherwise, check the threat type.
  return unsafe_resources.size() == 1 && !unsafe_resources[0].is_subresource;
}

SafeBrowsingBlockingPageV1::SafeBrowsingBlockingPageV1(
    SafeBrowsingUIManager* ui_manager,
    WebContents* web_contents,
    const UnsafeResourceList& unsafe_resources)
  : SafeBrowsingBlockingPage(ui_manager, web_contents, unsafe_resources) {
}

std::string SafeBrowsingBlockingPageV1::GetHTMLContents() {
  // Load the HTML page and create the template components.
  DictionaryValue strings;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::string html;

  if (unsafe_resources_.empty()) {
    NOTREACHED();
    return std::string();
  }

  DCHECK_GT(unsafe_resources_.size(), 1U);
  PopulateMultipleThreatStringDictionary(&strings);
  html = rb.GetRawDataResource(
      IDR_SAFE_BROWSING_MULTIPLE_THREAT_BLOCK).as_string();
  interstitial_show_time_ = base::TimeTicks::Now();
  return jstemplate_builder::GetTemplatesHtml(html, &strings, "template_root");
}

void SafeBrowsingBlockingPageV1::PopulateStringDictionary(
    DictionaryValue* strings,
    const string16& title,
    const string16& headline,
    const string16& description1,
    const string16& description2,
    const string16& description3) {
  strings->SetString("title", title);
  strings->SetString("headLine", headline);
  strings->SetString("description1", description1);
  strings->SetString("description2", description2);
  strings->SetString("description3", description3);
  strings->SetBoolean("proceedDisabled",
                      IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled));
}

void SafeBrowsingBlockingPageV1::PopulateMultipleThreatStringDictionary(
    DictionaryValue* strings) {

  string16 malware_label =
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_LABEL);
  string16 malware_link =
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_DIAGNOSTIC_PAGE);
  string16 phishing_label =
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_LABEL);
  string16 phishing_link =
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_REPORT_ERROR);

  ListValue* error_strings = new ListValue;
  for (UnsafeResourceList::const_iterator iter = unsafe_resources_.begin();
       iter != unsafe_resources_.end(); ++iter) {
    const UnsafeResource& resource = *iter;
    SBThreatType threat_type = resource.threat_type;
    DictionaryValue* current_error_strings = new DictionaryValue;
    if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
      current_error_strings->SetString("type", "malware");
      current_error_strings->SetString("typeLabel", malware_label);
      current_error_strings->SetString("errorLink", malware_link);
    } else {
      DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING ||
             threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL);
      current_error_strings->SetString("type", "phishing");
      current_error_strings->SetString("typeLabel", phishing_label);
      current_error_strings->SetString("errorLink", phishing_link);
    }
    current_error_strings->SetString("url", resource.url.spec());
    error_strings->Append(current_error_strings);
  }
  strings->Set("errors", error_strings);

  switch (interstitial_type_) {
    case TYPE_MALWARE_AND_PHISHING:
      PopulateStringDictionary(
          strings,
          // Use the malware headline, it is the scariest one.
          l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MULTI_THREAT_TITLE),
          l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_HEADLINE),
          l10n_util::GetStringFUTF16(
              IDS_SAFE_BROWSING_MULTI_THREAT_DESCRIPTION1,
              UTF8ToUTF16(web_contents_->GetURL().host())),
          l10n_util::GetStringUTF16(
              IDS_SAFE_BROWSING_MULTI_THREAT_DESCRIPTION2),
          string16());
      break;
    case TYPE_MALWARE:
      PopulateStringDictionary(
          strings,
          l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_TITLE),
          l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_HEADLINE),
          l10n_util::GetStringFUTF16(
              IDS_SAFE_BROWSING_MULTI_MALWARE_DESCRIPTION1,
              UTF8ToUTF16(web_contents_->GetURL().host())),
          l10n_util::GetStringUTF16(
              IDS_SAFE_BROWSING_MULTI_MALWARE_DESCRIPTION2),
          l10n_util::GetStringUTF16(
              IDS_SAFE_BROWSING_MULTI_MALWARE_DESCRIPTION3));
      break;
    case TYPE_PHISHING:
      PopulateStringDictionary(
          strings,
          l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_TITLE),
          l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_HEADLINE),
          l10n_util::GetStringFUTF16(
              IDS_SAFE_BROWSING_MULTI_PHISHING_DESCRIPTION1,
              UTF8ToUTF16(web_contents_->GetURL().host())),
          string16(),
          string16());
      break;
  }

  strings->SetString("confirm_text",
                     l10n_util::GetStringUTF16(
                         IDS_SAFE_BROWSING_MULTI_MALWARE_DESCRIPTION_AGREE));
  strings->SetString("continue_button",
                     l10n_util::GetStringUTF16(
                         IDS_SAFE_BROWSING_MULTI_MALWARE_PROCEED_BUTTON));
  strings->SetString("back_button",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_BACK_BUTTON));
  strings->SetString("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");
}

void SafeBrowsingBlockingPageV1::PopulateMalwareStringDictionary(
    DictionaryValue* strings) {
  NOTREACHED();
}

void SafeBrowsingBlockingPageV1::PopulatePhishingStringDictionary(
    DictionaryValue* strings) {
  NOTREACHED();
}

SafeBrowsingBlockingPageV2::SafeBrowsingBlockingPageV2(
    SafeBrowsingUIManager* ui_manager,
    WebContents* web_contents,
    const UnsafeResourceList& unsafe_resources)
  : SafeBrowsingBlockingPage(ui_manager, web_contents, unsafe_resources) {
}

std::string SafeBrowsingBlockingPageV2::GetHTMLContents() {
  // Load the HTML page and create the template components.
  DictionaryValue strings;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::string html;

  if (unsafe_resources_.empty()) {
    NOTREACHED();
    return std::string();
  }

  if (unsafe_resources_.size() > 1) {
    // TODO(mattm): Implement new multi-threat interstitial and remove
    // SafeBrowsingBlockingPageV1 entirely.  (http://crbug.com/160336)
    NOTREACHED();
  } else {
    SBThreatType threat_type = unsafe_resources_[0].threat_type;
    if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
      PopulateMalwareStringDictionary(&strings);
    } else {  // Phishing.
      DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING ||
             threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL);
      PopulatePhishingStringDictionary(&strings);
    }
    html = rb.GetRawDataResource(IDR_SAFE_BROWSING_MALWARE_BLOCK_V2).
        as_string();
  }
  interstitial_show_time_ = base::TimeTicks::Now();
  return jstemplate_builder::GetTemplatesHtml(html, &strings, "template-root");
}

void SafeBrowsingBlockingPageV2::PopulateStringDictionary(
    DictionaryValue* strings,
    const string16& title,
    const string16& headline,
    const string16& description1,
    const string16& description2,
    const string16& description3) {
  strings->SetString("title", title);
  strings->SetString("headLine", headline);
  strings->SetString("description1", description1);
  strings->SetString("description2", description2);
  strings->SetString("description3", description3);
  strings->SetBoolean("proceedDisabled",
                      IsPrefEnabled(prefs::kSafeBrowsingProceedAnywayDisabled));
  strings->SetBoolean("isMainFrame", is_main_frame_load_blocked_);
  strings->SetBoolean("isPhishing", interstitial_type_ == TYPE_PHISHING);

  strings->SetString("back_button",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_BACK_BUTTON));
  strings->SetString("seeMore", l10n_util::GetStringUTF16(
      IDS_SAFE_BROWSING_MALWARE_V2_SEE_MORE));
  strings->SetString("proceed",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_V2_PROCEED_LINK));

  web_ui_util::SetFontAndTextDirection(strings);
}

void SafeBrowsingBlockingPageV2::PopulateMultipleThreatStringDictionary(
    DictionaryValue* strings) {
  NOTREACHED();
}

void SafeBrowsingBlockingPageV2::PopulateMalwareStringDictionary(
    DictionaryValue* strings) {
  std::string diagnostic_link = base::StringPrintf(kSbDiagnosticHtml,
      l10n_util::GetStringUTF8(
        IDS_SAFE_BROWSING_MALWARE_DIAGNOSTIC_PAGE).c_str());

  // Check to see if we're blocking the main page, or a sub-resource on the
  // main page.
  string16 headline, description1, description2, description3;


  description3 = l10n_util::GetStringUTF16(
      IDS_SAFE_BROWSING_MALWARE_V2_DESCRIPTION3);
  if (is_main_frame_load_blocked_) {
    headline = l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_V2_HEADLINE);
    description1 = l10n_util::GetStringFUTF16(
        IDS_SAFE_BROWSING_MALWARE_V2_DESCRIPTION1,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
        UTF8ToUTF16(url_.host()));
    description2 = l10n_util::GetStringUTF16(
        IDS_SAFE_BROWSING_MALWARE_V2_DESCRIPTION2);
    strings->SetString("details", l10n_util::GetStringUTF16(
          IDS_SAFE_BROWSING_MALWARE_V2_DETAILS));
  } else {
    headline = l10n_util::GetStringUTF16(
        IDS_SAFE_BROWSING_MALWARE_V2_HEADLINE_SUBRESOURCE);
    description1 = l10n_util::GetStringFUTF16(
        IDS_SAFE_BROWSING_MALWARE_V2_DESCRIPTION1_SUBRESOURCE,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
        UTF8ToUTF16(web_contents_->GetURL().host()));
    description2 = l10n_util::GetStringFUTF16(
        IDS_SAFE_BROWSING_MALWARE_V2_DESCRIPTION2_SUBRESOURCE,
        UTF8ToUTF16(url_.host()));
    strings->SetString("details", l10n_util::GetStringFUTF16(
          IDS_SAFE_BROWSING_MALWARE_V2_DETAILS_SUBRESOURCE,
          UTF8ToUTF16(url_.host())));
  }

  PopulateStringDictionary(
      strings,
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_V2_TITLE),
      headline,
      description1,
      description2,
      description3);

  if (!CanShowMalwareDetailsOption()) {
    strings->SetBoolean(kDisplayCheckBox, false);
    strings->SetString("confirm_text", "");
    strings->SetString(kBoxChecked, "");
  } else {
    // Show the checkbox for sending malware details.
    strings->SetBoolean(kDisplayCheckBox, true);

    std::string privacy_link = base::StringPrintf(
        kPrivacyLinkHtml,
        l10n_util::GetStringUTF8(
            IDS_SAFE_BROWSING_PRIVACY_POLICY_PAGE_V2).c_str());

    strings->SetString("confirm_text",
                       l10n_util::GetStringFUTF16(
                           IDS_SAFE_BROWSING_MALWARE_V2_REPORTING_AGREE,
                           UTF8ToUTF16(privacy_link)));
    if (IsPrefEnabled(prefs::kSafeBrowsingReportingEnabled))
      strings->SetString(kBoxChecked, "yes");
    else
      strings->SetString(kBoxChecked, "");
  }

  strings->SetString("report_error", string16());
  strings->SetString("learnMore",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_V2_LEARN_MORE));
}

void SafeBrowsingBlockingPageV2::PopulatePhishingStringDictionary(
    DictionaryValue* strings) {
  PopulateStringDictionary(
      strings,
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_V2_TITLE),
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_V2_HEADLINE),
      l10n_util::GetStringFUTF16(IDS_SAFE_BROWSING_PHISHING_V2_DESCRIPTION1,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                                 UTF8ToUTF16(url_.host())),
      string16(),
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_V2_DESCRIPTION2));

  strings->SetString("details", "");
  strings->SetString("confirm_text", "");
  strings->SetString(kBoxChecked, "");
  strings->SetString("report_error",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_PHISHING_V2_REPORT_ERROR));
  strings->SetBoolean(kDisplayCheckBox, false);
  strings->SetString("learnMore",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_MALWARE_V2_LEARN_MORE));
}
