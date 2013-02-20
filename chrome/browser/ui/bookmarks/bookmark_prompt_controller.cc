// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_prompt_controller.h"

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_prompt_prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/variations/variation_ids.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

const char kBookmarkPromptTrialName[] = "BookmarkPrompt";
const char kBookmarkPromptDefaultGroup[] = "Disabled";
const char kBookmarkPromptControlGroup[] = "Control";
const char kBookmarkPromptExperimentGroup[] = "Experiment";

// This enum is used for the BookmarkPrompt.DisabledReason histogram.
enum PromptDisabledReason {
  PROMPT_DISABLED_REASON_BY_IMPRESSION_COUNT,
  PROMPT_DISABLED_REASON_BY_MANUAL,

  PROMPT_DISABLED_REASON_LIMIT, // Keep this last.
};

// This enum represents reason why we display bookmark prompt and for the
// BookmarkPrompt.DisplayReason histogram.
enum PromptDisplayReason {
  PROMPT_DISPLAY_REASON_NOT_DISPLAY, // We don't display the prompt.
  PROMPT_DISPLAY_REASON_PERMANENT,
  PROMPT_DISPLAY_REASON_SESSION,

  PROMPT_DISPLAY_REASON_LIMIT, // Keep this last.
};

// We enable bookmark prompt experiment for users who have profile created
// before |install_date| until |expiration_date|.
struct ExperimentDateRange {
  base::Time::Exploded install_date;
  base::Time::Exploded expiration_date;
};

bool CanShowBookmarkPrompt(Browser* browser) {
  BookmarkPromptPrefs prefs(browser->profile()->GetPrefs());
  if (!prefs.IsBookmarkPromptEnabled())
    return false;
  return prefs.GetPromptImpressionCount() <
         BookmarkPromptController::kMaxPromptImpressionCount;
}

const ExperimentDateRange* GetExperimentDateRange() {
  switch (chrome::VersionInfo::GetChannel()) {
    case chrome::VersionInfo::CHANNEL_BETA:
    case chrome::VersionInfo::CHANNEL_DEV: {
      // Experiment date range for M26 Beta/Dev
      static const ExperimentDateRange kBetaAndDevRange = {
        { 2013, 3, 0, 1, 0, 0, 0, 0 },   // Mar 1, 2013
        { 2013, 4, 0, 1, 0, 0, 0, 0 },   // Apr 1, 2013
      };
      return &kBetaAndDevRange;
    }
    case chrome::VersionInfo::CHANNEL_CANARY: {
      // Experiment date range for M26 Canary.
      static const ExperimentDateRange kCanaryRange = {
        { 2013, 1, 0, 17, 0, 0, 0, 0 },  // Jan 17, 2013
        { 2013, 2, 0, 18, 0, 0, 0, 0 },  // Feb 17, 2013
      };
      return &kCanaryRange;
    }
    case chrome::VersionInfo::CHANNEL_STABLE: {
      // Experiment date range for M25 Stable.
      static const ExperimentDateRange kStableRange = {
        { 2013, 2, 0, 25, 0, 0, 0, 0 },  // Feb 25, 2013
        { 2013, 3, 0, 28, 0, 0, 0, 0 },  // Mar 28, 2013
      };
      return &kStableRange;
    }
    default:
      return NULL;
  }
}

bool IsActiveWebContents(Browser* browser, WebContents* web_contents) {
  if (!browser->window()->IsActive())
    return false;
  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

bool IsBookmarked(Browser* browser, const GURL& url) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(
      browser->profile());
  return model && model->IsBookmarked(url);
}

bool IsEligiblePageTransitionForBookmarkPrompt(
    content::PageTransition type) {
  if (!content::PageTransitionIsMainFrame(type))
    return false;

  const content::PageTransition core_type =
      PageTransitionStripQualifier(type);

  if (core_type == content::PAGE_TRANSITION_RELOAD)
    return false;

  const int32 qualifier = content::PageTransitionGetQualifier(type);
  return !(qualifier & content::PAGE_TRANSITION_FORWARD_BACK);
}

// CheckPromptTriger returns prompt display reason based on |visits|.
PromptDisplayReason CheckPromptTriger(const history::VisitVector& visits) {
  const base::Time now = base::Time::Now();
  // We assume current visit is already in history database. Although, this
  // assumption may be false. We'll display prompt next time.
  int visit_permanent_count = 0;
  int visit_session_count = 0;
  for (history::VisitVector::const_iterator it = visits.begin();
       it != visits.end(); ++it) {
    if (!IsEligiblePageTransitionForBookmarkPrompt(it->transition))
      continue;
    ++visit_permanent_count;
    if ((now - it->visit_time) <= base::TimeDelta::FromDays(1))
      ++visit_session_count;
  }

  if (visit_permanent_count ==
      BookmarkPromptController::kVisitCountForPermanentTrigger)
    return PROMPT_DISPLAY_REASON_PERMANENT;

  if (visit_session_count ==
      BookmarkPromptController::kVisitCountForSessionTrigger)
    return PROMPT_DISPLAY_REASON_SESSION;

  return PROMPT_DISPLAY_REASON_NOT_DISPLAY;
}

}  // namespace

// BookmarkPromptController

// When impression count is greater than |kMaxPromptImpressionCount|, we
// don't display bookmark prompt anymore.
const int BookmarkPromptController::kMaxPromptImpressionCount = 5;

// When user visited the URL 10 times, we show the bookmark prompt.
const int BookmarkPromptController::kVisitCountForPermanentTrigger = 10;

// When user visited the URL 3 times last 24 hours, we show the bookmark
// prompt.
const int BookmarkPromptController::kVisitCountForSessionTrigger = 3;

BookmarkPromptController::BookmarkPromptController()
    : browser_(NULL),
      web_contents_(NULL) {
  DCHECK(browser_defaults::bookmarks_enabled);
  BrowserList::AddObserver(this);
}

BookmarkPromptController::~BookmarkPromptController() {
  BrowserList::RemoveObserver(this);
  SetBrowser(NULL);
}

// static
void BookmarkPromptController::AddedBookmark(Browser* browser,
                                             const GURL& url) {
  BookmarkPromptController* controller =
      g_browser_process->bookmark_prompt_controller();
  if (controller)
    controller->AddedBookmarkInternal(browser, url);
}

// static
void BookmarkPromptController::ClosingBookmarkPrompt() {
  BookmarkPromptController* controller =
      g_browser_process->bookmark_prompt_controller();
  if (controller)
    controller->ClosingBookmarkPromptInternal();
}

// static
void BookmarkPromptController::DisableBookmarkPrompt(
    PrefService* prefs) {
  UMA_HISTOGRAM_ENUMERATION("BookmarkPrompt.DisabledReason",
                            PROMPT_DISABLED_REASON_BY_MANUAL,
                            PROMPT_DISABLED_REASON_LIMIT);
  BookmarkPromptPrefs prompt_prefs(prefs);
  prompt_prefs.DisableBookmarkPrompt();
}

// Enable bookmark prompt controller feature for 1% of new users for one month
// on canary. We'll change the date for stable channel once release date fixed.
// static
bool BookmarkPromptController::IsEnabled() {
  // If manually create field trial available, we use it.
  const std::string manual_group_name = base::FieldTrialList::FindFullName(
      "BookmarkPrompt");
  if (!manual_group_name.empty())
    return manual_group_name == kBookmarkPromptExperimentGroup;

  const ExperimentDateRange* date_range = GetExperimentDateRange();
  if (!date_range)
    return false;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kBookmarkPromptTrialName, 100, kBookmarkPromptDefaultGroup,
          date_range->expiration_date.year,
          date_range->expiration_date.month,
          date_range->expiration_date.day_of_month, NULL));
  trial->UseOneTimeRandomization();
  trial->AppendGroup(kBookmarkPromptControlGroup, 99);
  trial->AppendGroup(kBookmarkPromptExperimentGroup, 1);

  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE,
      kBookmarkPromptTrialName, kBookmarkPromptDefaultGroup,
      chrome_variations::BOOKMARK_PROMPT_TRIAL_DEFAULT);
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE,
      kBookmarkPromptTrialName, kBookmarkPromptControlGroup,
      chrome_variations::BOOKMARK_PROMPT_TRIAL_CONTROL);
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE,
      kBookmarkPromptTrialName, kBookmarkPromptExperimentGroup,
      chrome_variations::BOOKMARK_PROMPT_TRIAL_EXPERIMENT);

  const base::Time start_date = base::Time::FromLocalExploded(
      date_range->install_date);
  const int64 install_time =
      g_browser_process->local_state()->GetInt64(prefs::kInstallDate);
  // This must be called after the pref is initialized.
  DCHECK(install_time);
  const base::Time install_date = base::Time::FromTimeT(install_time);

  if (install_date < start_date) {
    trial->Disable();
    return false;
  }
  return trial->group_name() == kBookmarkPromptExperimentGroup;
}

void BookmarkPromptController::ActiveTabChanged(WebContents* old_contents,
                                                WebContents* new_contents,
                                                int index,
                                                bool user_gesture) {
  SetWebContents(new_contents);
}

void BookmarkPromptController::AddedBookmarkInternal(Browser* browser,
                                                     const GURL& url) {
  if (browser == browser_ && url == last_prompted_url_) {
    last_prompted_url_ = GURL::EmptyGURL();
    UMA_HISTOGRAM_TIMES("BookmarkPrompt.AddedBookmark",
                        base::Time::Now() - last_prompted_time_);
  }
}

void BookmarkPromptController::ClosingBookmarkPromptInternal() {
  UMA_HISTOGRAM_TIMES("BookmarkPrompt.DisplayDuration",
                      base::Time::Now() - last_prompted_time_);
}

void BookmarkPromptController::Observe(
    int type,
    const content::NotificationSource&,
    const content::NotificationDetails&) {
  DCHECK_EQ(type, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
  query_url_consumer_.CancelAllRequests();
  if (!CanShowBookmarkPrompt(browser_))
    return;

  const GURL url = web_contents_->GetURL();
  if (!HistoryService::CanAddURL(url) || IsBookmarked(browser_, url))
    return;

  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      browser_->profile(),
      Profile::IMPLICIT_ACCESS);
  if (!history_service)
    return;

  query_url_start_time_ = base::Time::Now();
  history_service->QueryURL(
      url, true, &query_url_consumer_,
      base::Bind(&BookmarkPromptController::OnDidQueryURL,
                 base::Unretained(this)));
}

void BookmarkPromptController::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    SetBrowser(NULL);
}

void BookmarkPromptController::OnBrowserSetLastActive(Browser* browser) {
  if (browser && browser->type() == Browser::TYPE_TABBED &&
      !browser->profile()->IsOffTheRecord() &&
      browser->CanSupportWindowFeature(Browser::FEATURE_LOCATIONBAR) &&
      CanShowBookmarkPrompt(browser))
    SetBrowser(browser);
  else
    SetBrowser(NULL);
}

void BookmarkPromptController::OnDidQueryURL(
    CancelableRequestProvider::Handle handle,
    bool success,
    const history::URLRow* url_row,
    history::VisitVector* visits) {
  if (!success)
    return;

  const GURL url = web_contents_->GetURL();
  if (url_row->url() != url) {
    // The URL of web_contents_ is changed during QueryURL call. This is an
    // edge case but can be happened.
    return;
  }

  UMA_HISTOGRAM_TIMES("BookmarkPrompt.QueryURLDuration",
                      base::Time::Now() - query_url_start_time_);

  if (!browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR) ||
      !CanShowBookmarkPrompt(browser_) ||
      !IsActiveWebContents(browser_, web_contents_) ||
      IsBookmarked(browser_, url))
    return;

  PromptDisplayReason reason = CheckPromptTriger(*visits);
  UMA_HISTOGRAM_ENUMERATION("BookmarkPrompt.DisplayReason",
                            reason,
                            PROMPT_DISPLAY_REASON_LIMIT);
  if (reason == PROMPT_DISPLAY_REASON_NOT_DISPLAY)
    return;

  BookmarkPromptPrefs prefs(browser_->profile()->GetPrefs());
  prefs.IncrementPromptImpressionCount();
  if (prefs.GetPromptImpressionCount() == kMaxPromptImpressionCount) {
    UMA_HISTOGRAM_ENUMERATION("BookmarkPrompt.DisabledReason",
                              PROMPT_DISABLED_REASON_BY_IMPRESSION_COUNT,
                              PROMPT_DISABLED_REASON_LIMIT);
    prefs.DisableBookmarkPrompt();
  }
  last_prompted_time_ = base::Time::Now();
  last_prompted_url_ = web_contents_->GetURL();
  browser_->window()->ShowBookmarkPrompt();
}

void BookmarkPromptController::SetBrowser(Browser* browser) {
  if (browser_ == browser)
    return;
  if (browser_)
    browser_->tab_strip_model()->RemoveObserver(this);
  browser_ = browser;
  if (browser_)
    browser_->tab_strip_model()->AddObserver(this);
  SetWebContents(browser_ ? browser_->tab_strip_model()->GetActiveWebContents()
                          : NULL);
}

void BookmarkPromptController::SetWebContents(WebContents* web_contents) {
  if (web_contents_) {
    last_prompted_url_ = GURL::EmptyGURL();
    query_url_consumer_.CancelAllRequests();
    registrar_.Remove(
        this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::Source<WebContents>(web_contents_));
  }
  web_contents_ = web_contents;
  if (web_contents_) {
    registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                   content::Source<WebContents>(web_contents_));
  }
}
