// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_fetcher.h"
#include "grit/generated_resources.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

GoogleURLTrackerInfoBarDelegate* CreateInfoBar(
    InfoBarTabHelper* infobar_helper,
    const GURL& search_url,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url) {
  return new GoogleURLTrackerInfoBarDelegate(infobar_helper, search_url,
      google_url_tracker, new_google_url);
}

}  // namespace

// GoogleURLTrackerInfoBarDelegate --------------------------------------------

GoogleURLTrackerInfoBarDelegate::GoogleURLTrackerInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    const GURL& search_url,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url)
    : ConfirmInfoBarDelegate(infobar_helper),
      map_key_(infobar_helper),
      search_url_(search_url),
      google_url_tracker_(google_url_tracker),
      new_google_url_(new_google_url),
      showing_(false) {
}

bool GoogleURLTrackerInfoBarDelegate::Accept() {
  google_url_tracker_->AcceptGoogleURL(new_google_url_);
  return false;
}

bool GoogleURLTrackerInfoBarDelegate::Cancel() {
  google_url_tracker_->CancelGoogleURL(new_google_url_);
  return false;
}

string16 GoogleURLTrackerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GoogleURLTrackerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::OpenURLParams params(google_util::AppendGoogleLocaleParam(GURL(
      "https://www.google.com/support/chrome/bin/answer.py?answer=1618699")),
      content::Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->web_contents()->OpenURL(params);
  return false;
}

void GoogleURLTrackerInfoBarDelegate::Show() {
  showing_ = true;
  owner()->AddInfoBar(this);  // May delete |this| on failure!
}

void GoogleURLTrackerInfoBarDelegate::Close(bool redo_search) {
  if (redo_search) {
    // Re-do the user's search on the new domain.
    url_canon::Replacements<char> replacements;
    const std::string& host(new_google_url_.host());
    replacements.SetHost(host.data(), url_parse::Component(0, host.length()));
    GURL new_search_url(search_url_.ReplaceComponents(replacements));
    if (new_search_url.is_valid()) {
      content::OpenURLParams params(new_search_url, content::Referrer(),
          CURRENT_TAB, content::PAGE_TRANSITION_GENERATED, false);
      owner()->web_contents()->OpenURL(params);
    }
  }

  if (!showing_) {
    // We haven't been added to a tab, so just delete ourselves.
    delete this;
    return;
  }

  // Synchronously remove ourselves from the URL tracker's list, because the
  // RemoveInfoBar() call below may result in either a synchronous or an
  // asynchronous call back to InfoBarClosed(), and it's easier to handle when
  // we just guarantee the removal is synchronous.
  google_url_tracker_->InfoBarClosed(map_key_);
  google_url_tracker_ = NULL;

  // If we're already animating closed, we won't have an owner.  Do nothing in
  // this case.
  // TODO(pkasting): For now, this can also happen if we were showing in a
  // background tab that was then closed, in which case we'll have leaked and
  // subsequently reached here due to GoogleURLTracker::CloseAllInfoBars().
  // This case will no longer happen once infobars are refactored to own their
  // delegates.
  if (owner())
    owner()->RemoveInfoBar(this);
}

GoogleURLTrackerInfoBarDelegate::~GoogleURLTrackerInfoBarDelegate() {
  if (google_url_tracker_)
    google_url_tracker_->InfoBarClosed(map_key_);
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
      (new_host ? new_google_url_ : google_url_tracker_->google_url_).host()));
}


// GoogleURLTracker -----------------------------------------------------------

const char GoogleURLTracker::kDefaultGoogleHomepage[] =
    "http://www.google.com/";
const char GoogleURLTracker::kSearchDomainCheckURL[] =
    "https://www.google.com/searchdomaincheck?format=domain&type=chrome";

GoogleURLTracker::GoogleURLTracker(Mode mode)
    : infobar_creator_(&CreateInfoBar),
      google_url_(mode == UNIT_TEST_MODE ? kDefaultGoogleHomepage :
          g_browser_process->local_state()->GetString(
              prefs::kLastKnownGoogleURL)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      fetcher_id_(0),
      in_startup_sleep_(true),
      already_fetched_(false),
      need_to_fetch_(false),
      need_to_prompt_(false) {
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay five seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  //
  // In UNIT_TEST mode, where we want to explicitly control when the tracker
  // "wakes up", we do nothing at all.
  if (mode == NORMAL_MODE) {
    static const int kStartFetchDelayMS = 5000;
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&GoogleURLTracker::FinishSleep,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kStartFetchDelayMS));
  }
}

GoogleURLTracker::~GoogleURLTracker() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  // We should only reach here after any tabs and their infobars have been torn
  // down.
  DCHECK(infobar_map_.empty());
}

// static
GURL GoogleURLTracker::GoogleURL() {
  const GoogleURLTracker* tracker = g_browser_process->google_url_tracker();
  return tracker ? tracker->google_url_ : GURL(kDefaultGoogleHomepage);
}

// static
void GoogleURLTracker::RequestServerCheck() {
  GoogleURLTracker* tracker = g_browser_process->google_url_tracker();
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

void GoogleURLTracker::AcceptGoogleURL(const GURL& new_google_url) {
  google_url_ = new_google_url;
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kLastKnownGoogleURL, google_url_.spec());
  prefs->SetString(prefs::kLastPromptedGoogleURL, google_url_.spec());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
  need_to_prompt_ = false;
  CloseAllInfoBars(true);
}

void GoogleURLTracker::CancelGoogleURL(const GURL& new_google_url) {
  g_browser_process->local_state()->SetString(prefs::kLastPromptedGoogleURL,
                                              new_google_url.spec());
  need_to_prompt_ = false;
  CloseAllInfoBars(false);
}

void GoogleURLTracker::InfoBarClosed(const InfoBarTabHelper* infobar_helper) {
  InfoBarMap::iterator i(infobar_map_.find(infobar_helper));
  DCHECK(i != infobar_map_.end());
  infobar_map_.erase(i);
}

void GoogleURLTracker::SetNeedToFetch() {
  need_to_fetch_ = true;
  StartFetchIfDesirable();
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

  std::string fetch_url = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kGoogleSearchDomainCheckURL);
  if (fetch_url.empty())
    fetch_url = kSearchDomainCheckURL;

  already_fetched_ = true;
  fetcher_.reset(content::URLFetcher::Create(fetcher_id_, GURL(fetch_url),
                                             content::URLFetcher::GET, this));
  ++fetcher_id_;
  // We don't want this fetch to set new entries in the cache or cookies, lest
  // we alarm the user.
  fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetRequestContext(g_browser_process->system_request_context());

  // Configure to max_retries at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher_->SetMaxRetries(kMaxRetries);

  fetcher_->Start();
}

void GoogleURLTracker::OnURLFetchComplete(const content::URLFetcher* source) {
  // Delete the fetcher on this function's exit.
  scoped_ptr<content::URLFetcher> clean_up_fetcher(fetcher_.release());

  // Don't update the URL if the request didn't succeed.
  if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200)) {
    already_fetched_ = false;
    return;
  }

  // See if the response data was one we want to use, and if so, convert to the
  // appropriate Google base URL.
  std::string url_str;
  source->GetResponseAsString(&url_str);
  TrimWhitespace(url_str, TRIM_ALL, &url_str);

  if (!StartsWithASCII(url_str, ".google.", false))
    return;

  fetched_google_url_ = GURL("http://www" + url_str);
  GURL last_prompted_url(
      g_browser_process->local_state()->GetString(
          prefs::kLastPromptedGoogleURL));

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

  // Any open infobars are pointing at the wrong Google URL.  (This can happen
  // if an infobar has been sitting open and then our IP address changes.)
  CloseAllInfoBars(false);
}

void GoogleURLTracker::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      content::WebContents* contents = controller->GetWebContents();
      OnNavigationPending(source,
          content::Source<content::WebContents>(contents),
          TabContentsWrapper::GetCurrentWrapperForContents(contents)->
              infobar_tab_helper(), controller->GetPendingEntry()->GetURL());
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      content::WebContents* contents =
          content::Source<content::NavigationController>(source)->
              GetWebContents();
      OnNavigationCommittedOrTabClosed(source,
          content::Source<content::WebContents>(contents),
          TabContentsWrapper::GetCurrentWrapperForContents(contents)->
              infobar_tab_helper(), true);
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      content::WebContents* contents =
          content::Source<content::WebContents>(source).ptr();
      OnNavigationCommittedOrTabClosed(
          content::Source<content::NavigationController>(&contents->
              GetController()), source,
          TabContentsWrapper::GetCurrentWrapperForContents(contents)->
              infobar_tab_helper(), false);
      break;
    }

    case chrome::NOTIFICATION_INSTANT_COMMITTED: {
      TabContentsWrapper* wrapper =
          content::Source<TabContentsWrapper>(source).ptr();
      content::WebContents* contents = wrapper->web_contents();
      content::Source<content::NavigationController> source(
          &contents->GetController());
      content::Source<content::WebContents> contents_source(contents);
      InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
      OnNavigationPending(source, contents_source, infobar_helper,
                          contents->GetURL());
      OnNavigationCommittedOrTabClosed(source, contents_source, infobar_helper,
                                       true);
      break;
    }

    default:
      NOTREACHED() << "Unknown notification received:" << type;
  }
}

void GoogleURLTracker::OnIPAddressChanged() {
  already_fetched_ = false;
  StartFetchIfDesirable();
}

void GoogleURLTracker::SearchCommitted() {
  if (need_to_prompt_) {
    // This notification will fire a bit later in the same call chain we're
    // currently in.
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::NotificationService::AllBrowserContextsAndSources());
    registrar_.Add(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::NotificationService::AllBrowserContextsAndSources());
  }
}

void GoogleURLTracker::OnNavigationPending(
    const content::NotificationSource& navigation_controller_source,
    const content::NotificationSource& web_contents_source,
    InfoBarTabHelper* infobar_helper,
    const GURL& search_url) {
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllBrowserContextsAndSources());

  if (registrar_.IsRegistered(this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED, web_contents_source)) {
    // If the previous load hasn't committed and the user triggers a new load,
    // we don't need to re-register our listeners; just kill the old,
    // never-shown infobar (to be replaced by a new one below).
    InfoBarMap::iterator i(infobar_map_.find(infobar_helper));
    DCHECK(i != infobar_map_.end());
    i->second->Close(false);
  } else {
    // Start listening for the commit notification. We also need to listen for
    // the tab close command since that means the load will never commit.  Note
    // that in this case we don't need to close any previous infobar for this
    // tab since this navigation will close it.
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   navigation_controller_source);
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   web_contents_source);
  }

  infobar_map_[infobar_helper] = (*infobar_creator_)(infobar_helper, search_url,
                                                     this, fetched_google_url_);
}

void GoogleURLTracker::OnNavigationCommittedOrTabClosed(
    const content::NotificationSource& navigation_controller_source,
    const content::NotificationSource& web_contents_source,
    const InfoBarTabHelper* infobar_helper,
    bool navigated) {
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                    navigation_controller_source);
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    web_contents_source);

  InfoBarMap::iterator i(infobar_map_.find(infobar_helper));
  DCHECK(i != infobar_map_.end());
  DCHECK(need_to_prompt_);
  if (navigated)
    i->second->Show();
  else
    i->second->Close(false);  // Close manually since it's not added to a tab.
}

void GoogleURLTracker::CloseAllInfoBars(bool redo_searches) {
  // Close all infobars, whether they've been added to tabs or not.
  while (!infobar_map_.empty())
    infobar_map_.begin()->second->Close(redo_searches);

  // Any registered listeners for NAV_ENTRY_COMMITTED and TAB_CLOSED are now
  // irrelevant as the associated infobars are gone.
  registrar_.RemoveAll();
}
