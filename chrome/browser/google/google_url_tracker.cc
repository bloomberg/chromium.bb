// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

InfoBarDelegate* CreateInfobar(TabContents* tab_contents,
                               GoogleURLTracker* google_url_tracker,
                               const GURL& new_google_url) {
  InfoBarDelegate* infobar = new GoogleURLTrackerInfoBarDelegate(tab_contents,
      google_url_tracker, new_google_url);
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  wrapper->infobar_tab_helper()->AddInfoBar(infobar);
  return infobar;
}

}  // namespace

// GoogleURLTrackerInfoBarDelegate --------------------------------------------

GoogleURLTrackerInfoBarDelegate::GoogleURLTrackerInfoBarDelegate(
    TabContents* tab_contents,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url)
    : ConfirmInfoBarDelegate(tab_contents),
      google_url_tracker_(google_url_tracker),
      new_google_url_(new_google_url),
      tab_contents_(tab_contents) {
}

bool GoogleURLTrackerInfoBarDelegate::Accept() {
  google_url_tracker_->AcceptGoogleURL(new_google_url_);
  google_url_tracker_->RedoSearch();
  return true;
}

bool GoogleURLTrackerInfoBarDelegate::Cancel() {
  google_url_tracker_->CancelGoogleURL(new_google_url_);
  return true;
}

string16 GoogleURLTrackerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GoogleURLTrackerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  tab_contents_->OpenURL(google_util::AppendGoogleLocaleParam(GURL(
      "https://www.google.com/support/chrome/bin/answer.py?answer=1618699")),
      GURL(), (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      PageTransition::LINK);
  return false;
}

GoogleURLTrackerInfoBarDelegate::~GoogleURLTrackerInfoBarDelegate() {
  google_url_tracker_->InfoBarClosed();
}

string16 GoogleURLTrackerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GOOGLE_URL_TRACKER_INFOBAR_MESSAGE,
                                    GetHost(true), GetHost(false));
}

string16 GoogleURLTrackerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  bool new_host = (button == BUTTON_OK);
  return l10n_util::GetStringFUTF16(new_host ?
      IDS_GOOGLE_URL_TRACKER_INFOBAR_SWITCH :
      IDS_GOOGLE_URL_TRACKER_INFOBAR_DONT_SWITCH, GetHost(new_host));
}

string16 GoogleURLTrackerInfoBarDelegate::GetHost(bool new_host) const {
  return net::StripWWW(UTF8ToUTF16(
      (new_host ? new_google_url_ : google_url_tracker_->GoogleURL()).host()));
}


// GoogleURLTracker -----------------------------------------------------------

const char GoogleURLTracker::kDefaultGoogleHomepage[] =
    "http://www.google.com/";
const char GoogleURLTracker::kSearchDomainCheckURL[] =
    "https://www.google.com/searchdomaincheck?format=domain&type=chrome";

GoogleURLTracker::GoogleURLTracker()
    : infobar_creator_(&CreateInfobar),
      google_url_(g_browser_process->local_state()->GetString(
          prefs::kLastKnownGoogleURL)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      fetcher_id_(0),
      queue_wakeup_task_(true),
      in_startup_sleep_(true),
      already_fetched_(false),
      need_to_fetch_(false),
      need_to_prompt_(false),
      controller_(NULL),
      infobar_(NULL) {
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&GoogleURLTracker::QueueWakeupTask,
                 weak_ptr_factory_.GetWeakPtr()));
}

GoogleURLTracker::~GoogleURLTracker() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

// static
GURL GoogleURLTracker::GoogleURL() {
  const GoogleURLTracker* const tracker =
      g_browser_process->google_url_tracker();
  return tracker ? tracker->google_url_ : GURL(kDefaultGoogleHomepage);
}

// static
void GoogleURLTracker::RequestServerCheck() {
  GoogleURLTracker* const tracker = g_browser_process->google_url_tracker();
  if (tracker)
    tracker->SetNeedToFetch();
}

// static
void GoogleURLTracker::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kLastKnownGoogleURL,
                            kDefaultGoogleHomepage);
  prefs->RegisterStringPref(prefs::kLastPromptedGoogleURL, std::string());
}

// static
void GoogleURLTracker::GoogleURLSearchCommitted() {
  GoogleURLTracker* tracker = g_browser_process->google_url_tracker();
  if (tracker)
    tracker->SearchCommitted();
}

void GoogleURLTracker::SetNeedToFetch() {
  need_to_fetch_ = true;
  StartFetchIfDesirable();
}

void GoogleURLTracker::QueueWakeupTask() {
  // When testing, we want to wake from sleep at controlled times, not on a
  // timer.
  if (!queue_wakeup_task_)
    return;

  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay five seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  static const int kStartFetchDelayMS = 5000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&GoogleURLTracker::FinishSleep,
                 weak_ptr_factory_.GetWeakPtr()),
      kStartFetchDelayMS);
}

void GoogleURLTracker::FinishSleep() {
  in_startup_sleep_ = false;
  StartFetchIfDesirable();
}

void GoogleURLTracker::StartFetchIfDesirable() {
  // Bail if a fetch isn't appropriate right now.  This function will be called
  // again each time one of the preconditions changes, so we'll fetch
  // immediately once all of them are met.
  //
  // See comments in header on the class, on RequestServerCheck(), and on the
  // various members here for more detail on exactly what the conditions are.
  if (in_startup_sleep_ || already_fetched_ || !need_to_fetch_)
    return;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBackgroundNetworking))
    return;

  already_fetched_ = true;
  fetcher_.reset(URLFetcher::Create(fetcher_id_, GURL(kSearchDomainCheckURL),
                                    URLFetcher::GET, this));
  ++fetcher_id_;
  // We don't want this fetch to affect existing state in local_state.  For
  // example, if a user has no Google cookies, this automatic check should not
  // cause one to be set, lest we alarm the user.
  fetcher_->set_load_flags(net::LOAD_DISABLE_CACHE |
                           net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->set_request_context(g_browser_process->system_request_context());

  // Configure to max_retries at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher_->set_max_retries(kMaxRetries);

  fetcher_->Start();
}

void GoogleURLTracker::OnURLFetchComplete(const URLFetcher* source,
                                          const GURL& url,
                                          const net::URLRequestStatus& status,
                                          int response_code,
                                          const net::ResponseCookies& cookies,
                                          const std::string& data) {
  // Delete the fetcher on this function's exit.
  scoped_ptr<URLFetcher> clean_up_fetcher(fetcher_.release());

  // Don't update the URL if the request didn't succeed.
  if (!status.is_success() || (response_code != 200)) {
    already_fetched_ = false;
    return;
  }

  // See if the response data was one we want to use, and if so, convert to the
  // appropriate Google base URL.
  std::string url_str;
  TrimWhitespace(data, TRIM_ALL, &url_str);

  if (!StartsWithASCII(url_str, ".google.", false))
    return;

  fetched_google_url_ = GURL("http://www" + url_str);
  GURL last_prompted_url(
      g_browser_process->local_state()->GetString(
          prefs::kLastPromptedGoogleURL));
  need_to_prompt_ = false;

  if (last_prompted_url.is_empty()) {
    // On the very first run of Chrome, when we've never looked up the URL at
    // all, we should just silently switch over to whatever we get immediately.
    AcceptGoogleURL(fetched_google_url_);
    return;
  }

  // If the URL hasn't changed, then whether |need_to_prompt_| is true or false,
  // nothing has changed, so just bail.
  if (fetched_google_url_ == last_prompted_url)
    return;

  if (fetched_google_url_ == google_url_) {
    // The user came back to their original location after having temporarily
    // moved.  Reset the prompted URL so we'll prompt again if they move again.
    CancelGoogleURL(fetched_google_url_);
    return;
  }

  need_to_prompt_ = true;
}

void GoogleURLTracker::AcceptGoogleURL(const GURL& new_google_url) {
  google_url_ = new_google_url;
  g_browser_process->local_state()->SetString(prefs::kLastKnownGoogleURL,
                                              google_url_.spec());
  g_browser_process->local_state()->SetString(prefs::kLastPromptedGoogleURL,
                                              google_url_.spec());
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
  need_to_prompt_ = false;
}

void GoogleURLTracker::CancelGoogleURL(const GURL& new_google_url) {
  g_browser_process->local_state()->SetString(prefs::kLastPromptedGoogleURL,
                                              new_google_url.spec());
  need_to_prompt_ = false;
}

void GoogleURLTracker::InfoBarClosed() {
  registrar_.RemoveAll();
  controller_ = NULL;
  infobar_ = NULL;
  search_url_ = GURL();
}

void GoogleURLTracker::RedoSearch() {
  //  Re-do the user's search on the new domain.
  DCHECK(controller_);
  url_canon::Replacements<char> replacements;
  replacements.SetHost(google_url_.host().data(),
                       url_parse::Component(0, google_url_.host().length()));
  GURL new_search_url(search_url_.ReplaceComponents(replacements));
  if (new_search_url.is_valid())
    controller_->tab_contents()->OpenURL(new_search_url, GURL(), CURRENT_TAB,
                                         PageTransition::GENERATED);
}

void GoogleURLTracker::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      OnNavigationPending(source, controller->pending_entry()->url());
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
    case content::NOTIFICATION_TAB_CLOSED:
      OnNavigationCommittedOrTabClosed(
          Source<NavigationController>(source).ptr()->tab_contents(),
          type);
      break;

    default:
      NOTREACHED() << "Unknown notification received:" << type;
  }
}

void GoogleURLTracker::OnIPAddressChanged() {
  already_fetched_ = false;
  StartFetchIfDesirable();
}

void GoogleURLTracker::SearchCommitted() {
  if (registrar_.IsEmpty() && (need_to_prompt_ || fetcher_.get())) {
    // This notification will fire a bit later in the same call chain we're
    // currently in.
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                   NotificationService::AllSources());
  }
}

void GoogleURLTracker::OnNavigationPending(const NotificationSource& source,
                                           const GURL& pending_url) {
  controller_ = Source<NavigationController>(source).ptr();
  search_url_ = pending_url;
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                    NotificationService::AllSources());
  // Start listening for the commit notification. We also need to listen for the
  // tab close command since that means the load will never commit.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, content::NOTIFICATION_TAB_CLOSED,
                 Source<NavigationController>(controller_));
}

void GoogleURLTracker::OnNavigationCommittedOrTabClosed(
    TabContents* tab_contents,
    int type) {
  registrar_.RemoveAll();

  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    ShowGoogleURLInfoBarIfNecessary(tab_contents);
  } else {
    controller_ = NULL;
    infobar_ = NULL;
  }
}

void GoogleURLTracker::ShowGoogleURLInfoBarIfNecessary(
    TabContents* tab_contents) {
  if (!need_to_prompt_)
    return;
  DCHECK(!fetched_google_url_.is_empty());

  infobar_ = (*infobar_creator_)(tab_contents, this, fetched_google_url_);
}
