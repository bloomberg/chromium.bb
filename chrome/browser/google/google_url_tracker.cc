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
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

GoogleURLTrackerInfoBarDelegate* CreateInfoBar(
    InfoBarTabHelper* infobar_helper,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url) {
  return new GoogleURLTrackerInfoBarDelegate(infobar_helper, google_url_tracker,
                                             new_google_url);
}

string16 GetHost(const GURL& url) {
  DCHECK(url.is_valid());
  return net::StripWWW(UTF8ToUTF16(url.host()));
}

}  // namespace

// GoogleURLTrackerInfoBarDelegate --------------------------------------------

GoogleURLTrackerInfoBarDelegate::GoogleURLTrackerInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url)
    : ConfirmInfoBarDelegate(infobar_helper),
      map_key_(infobar_helper),
      google_url_tracker_(google_url_tracker),
      new_google_url_(new_google_url),
      showing_(false),
      pending_id_(0) {
}

bool GoogleURLTrackerInfoBarDelegate::Accept() {
  google_url_tracker_->AcceptGoogleURL(new_google_url_, true);
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
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

bool GoogleURLTrackerInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  int unique_id = details.entry->GetUniqueID();
  return (unique_id != contents_unique_id()) && (unique_id != pending_id_);
}

void GoogleURLTrackerInfoBarDelegate::SetGoogleURL(const GURL& new_google_url) {
  DCHECK_EQ(GetHost(new_google_url_), GetHost(new_google_url));
  new_google_url_ = new_google_url;
}

void GoogleURLTrackerInfoBarDelegate::Show(const GURL& search_url) {
  if (!owner())
    return;
  StoreActiveEntryUniqueID(owner());
  search_url_ = search_url;
  pending_id_ = 0;
  if (!showing_) {
    showing_ = true;
    owner()->AddInfoBar(this);  // May delete |this| on failure!
  }
}

void GoogleURLTrackerInfoBarDelegate::Close(bool redo_search) {
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
  if (!owner())
    return;

  if (redo_search) {
    // Re-do the user's search on the new domain.
    DCHECK(search_url_.is_valid());
    url_canon::Replacements<char> replacements;
    const std::string& host(new_google_url_.host());
    replacements.SetHost(host.data(), url_parse::Component(0, host.length()));
    GURL new_search_url(search_url_.ReplaceComponents(replacements));
    if (new_search_url.is_valid()) {
      content::OpenURLParams params(new_search_url, content::Referrer(),
          CURRENT_TAB, content::PAGE_TRANSITION_GENERATED, false);
      owner()->GetWebContents()->OpenURL(params);
    }
  }

  owner()->RemoveInfoBar(this);
}

GoogleURLTrackerInfoBarDelegate::~GoogleURLTrackerInfoBarDelegate() {
  if (google_url_tracker_)
    google_url_tracker_->InfoBarClosed(map_key_);
}

string16 GoogleURLTrackerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GOOGLE_URL_TRACKER_INFOBAR_MESSAGE,
      GetHost(new_google_url_), GetHost(google_url_tracker_->google_url_));
}

string16 GoogleURLTrackerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  bool new_host = (button == BUTTON_OK);
  return l10n_util::GetStringFUTF16(new_host ?
      IDS_GOOGLE_URL_TRACKER_INFOBAR_SWITCH :
      IDS_GOOGLE_URL_TRACKER_INFOBAR_DONT_SWITCH,
      GetHost(new_host ? new_google_url_ : google_url_tracker_->google_url_));
}


// GoogleURLTracker::MapEntry -------------------------------------------------

// Note that we have to initialize at least the NotificationSources explicitly
// lest this not compile, because NotificationSource has no null constructor.
GoogleURLTracker::MapEntry::MapEntry()
    : infobar(NULL),
      navigation_controller_source(
          content::Source<content::NavigationController>(NULL)),
      tab_contents_source(content::Source<TabContents>(NULL)) {
  NOTREACHED();
}

GoogleURLTracker::MapEntry::MapEntry(
    GoogleURLTrackerInfoBarDelegate* infobar,
    const content::NotificationSource& navigation_controller_source,
    const content::NotificationSource& tab_contents_source)
    : infobar(infobar),
      navigation_controller_source(navigation_controller_source),
      tab_contents_source(tab_contents_source) {
}

GoogleURLTracker::MapEntry::~MapEntry() {
}


// GoogleURLTracker -----------------------------------------------------------

const char GoogleURLTracker::kDefaultGoogleHomepage[] =
    "http://www.google.com/";
const char GoogleURLTracker::kSearchDomainCheckURL[] =
    "https://www.google.com/searchdomaincheck?format=url&type=chrome";

GoogleURLTracker::GoogleURLTracker(Profile* profile, Mode mode)
    : profile_(profile),
      infobar_creator_(&CreateInfoBar),
      google_url_(mode == UNIT_TEST_MODE ? kDefaultGoogleHomepage :
          profile->GetPrefs()->GetString(prefs::kLastKnownGoogleURL)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      fetcher_id_(0),
      in_startup_sleep_(true),
      already_fetched_(false),
      need_to_fetch_(false),
      need_to_prompt_(false),
      search_committed_(false) {
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
  // We should only reach here after any tabs and their infobars have been torn
  // down.
  DCHECK(infobar_map_.empty());
}

// static
GURL GoogleURLTracker::GoogleURL(Profile* profile) {
  const GoogleURLTracker* tracker =
      GoogleURLTrackerFactory::GetForProfile(profile);
  return tracker ? tracker->google_url_ : GURL(kDefaultGoogleHomepage);
}

// static
void GoogleURLTracker::RequestServerCheck(Profile* profile) {
  GoogleURLTracker* tracker = GoogleURLTrackerFactory::GetForProfile(profile);
  if (tracker)
    tracker->SetNeedToFetch();
}

// static
void GoogleURLTracker::GoogleURLSearchCommitted(Profile* profile) {
  GoogleURLTracker* tracker = GoogleURLTrackerFactory::GetForProfile(profile);
  if (tracker)
    tracker->SearchCommitted();
}

void GoogleURLTracker::OnURLFetchComplete(const net::URLFetcher* source) {
  // Delete the fetcher on this function's exit.
  scoped_ptr<net::URLFetcher> clean_up_fetcher(fetcher_.release());

  // Don't update the URL if the request didn't succeed.
  if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200)) {
    already_fetched_ = false;
    return;
  }

  // See if the response data was valid.  It should be
  // "<scheme>://[www.]google.<TLD>/".
  std::string url_str;
  source->GetResponseAsString(&url_str);
  TrimWhitespace(url_str, TRIM_ALL, &url_str);
  GURL url(url_str);
  if (!url.is_valid() || (url.path().length() > 1) || url.has_query() ||
      url.has_ref() ||
      !google_util::IsGoogleDomainUrl(url.spec(),
                                      google_util::DISALLOW_SUBDOMAIN,
                                      google_util::DISALLOW_NON_STANDARD_PORTS))
    return;

  std::swap(url, fetched_google_url_);
  GURL last_prompted_url(
      profile_->GetPrefs()->GetString(prefs::kLastPromptedGoogleURL));

  if (last_prompted_url.is_empty()) {
    // On the very first run of Chrome, when we've never looked up the URL at
    // all, we should just silently switch over to whatever we get immediately.
    AcceptGoogleURL(fetched_google_url_, true);  // Second arg is irrelevant.
    return;
  }

  string16 fetched_host(GetHost(fetched_google_url_));
  if (fetched_google_url_ == google_url_) {
    // Either the user has continually been on this URL, or we prompted for a
    // different URL but have now changed back before they responded to any of
    // the prompts.  In this latter case we want to close any open infobars and
    // stop prompting.
    CancelGoogleURL(fetched_google_url_);
  } else if (fetched_host == GetHost(google_url_)) {
    // Similar to the above case, but this time the new URL differs from the
    // existing one, probably due to switching between HTTP and HTTPS searching.
    // Like before we want to close any open infobars and stop prompting; we
    // also want to silently accept the change in scheme.  We don't redo open
    // searches so as to avoid suddenly changing a page the user might be
    // interacting with; it's enough to simply get future searches right.
    AcceptGoogleURL(fetched_google_url_, false);
  } else if (fetched_host == GetHost(last_prompted_url)) {
    // We've re-fetched a TLD the user previously turned down.  Although the new
    // URL might have a different scheme than the old, we want to preserve the
    // user's decision.  Note that it's possible that, like in the above two
    // cases, we fetched yet another different URL in the meantime, which we
    // have open infobars prompting about; in this case, as in those above, we
    // want to go ahead and close the infobars and stop prompting, since we've
    // switched back away from that URL.
    CancelGoogleURL(fetched_google_url_);
  } else {
    // We've fetched a URL with a different TLD than the user is currently using
    // or was previously prompted about.  This means we need to prompt again.
    need_to_prompt_ = true;

    // As in all the above cases, there could be open infobars prompting about
    // some URL.  If these URLs have the same TLD, we can simply leave the
    // existing infobars open and quietly point their "new Google URL"s at the
    // new URL (for e.g. scheme changes).  Otherwise we go ahead and close the
    // existing infobars since their message is out-of-date.
    if (!url.is_valid())  // Note: |url| is the previous |fetched_google_url_|.
      return;
    if (fetched_host != GetHost(url)) {
      CloseAllInfoBars(false);
    } else if (fetched_google_url_ != url) {
      for (InfoBarMap::iterator i(infobar_map_.begin());
           i != infobar_map_.end(); ++i)
        i->second.infobar->SetGoogleURL(fetched_google_url_);
    }
  }
}

void GoogleURLTracker::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      // Because we're listening to all sources, there may be no TabContents for
      // some notifications, e.g. navigations in bubbles/balloons etc.  See
      // comments in tab_contents.h.
      TabContents* tab_contents =
          TabContents::FromWebContents(controller->GetWebContents());
      if (tab_contents) {
        OnNavigationPending(source, content::Source<TabContents>(tab_contents),
                            tab_contents->infobar_tab_helper(),
                            controller->GetPendingEntry()->GetUniqueID());
      }
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      // Here we're only listening to notifications where we already know
      // there's an associated TabContents.
      TabContents* tab_contents =
          TabContents::FromWebContents(controller->GetWebContents());
      DCHECK(tab_contents);
      OnNavigationCommittedOrTabClosed(tab_contents->infobar_tab_helper(),
                                       controller->GetActiveEntry()->GetURL());
      break;
    }

    case chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      OnNavigationCommittedOrTabClosed(
          content::Source<TabContents>(source)->infobar_tab_helper(), GURL());
      break;
    }

    case chrome::NOTIFICATION_INSTANT_COMMITTED: {
      TabContents* tab_contents = content::Source<TabContents>(source).ptr();
      content::WebContents* web_contents = tab_contents->web_contents();
      OnInstantCommitted(
          content::Source<content::NavigationController>(
              &web_contents->GetController()),
          source, tab_contents->infobar_tab_helper(), web_contents->GetURL());
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

void GoogleURLTracker::Shutdown() {
  registrar_.RemoveAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
  fetcher_.reset();
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void GoogleURLTracker::AcceptGoogleURL(const GURL& new_google_url,
                                       bool redo_searches) {
  UpdatedDetails urls(google_url_, new_google_url);
  google_url_ = new_google_url;
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetString(prefs::kLastKnownGoogleURL, google_url_.spec());
  prefs->SetString(prefs::kLastPromptedGoogleURL, google_url_.spec());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      content::Source<Profile>(profile_),
      content::Details<UpdatedDetails>(&urls));
  need_to_prompt_ = false;
  CloseAllInfoBars(redo_searches);
}

void GoogleURLTracker::CancelGoogleURL(const GURL& new_google_url) {
  profile_->GetPrefs()->SetString(prefs::kLastPromptedGoogleURL,
                                  new_google_url.spec());
  need_to_prompt_ = false;
  CloseAllInfoBars(false);
}

void GoogleURLTracker::InfoBarClosed(const InfoBarTabHelper* infobar_helper) {
  InfoBarMap::iterator i(infobar_map_.find(infobar_helper));
  DCHECK(i != infobar_map_.end());

  UnregisterForEntrySpecificNotifications(i->second, false);
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
  fetcher_.reset(net::URLFetcher::Create(fetcher_id_, GURL(fetch_url),
                                         net::URLFetcher::GET, this));
  ++fetcher_id_;
  // We don't want this fetch to set new entries in the cache or cookies, lest
  // we alarm the user.
  fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetRequestContext(profile_->GetRequestContext());

  // Configure to max_retries at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher_->SetMaxRetries(kMaxRetries);

  fetcher_->Start();
}

void GoogleURLTracker::SearchCommitted() {
  if (need_to_prompt_) {
    search_committed_ = true;
    // These notifications will fire a bit later in the same call chain we're
    // currently in.
    if (!registrar_.IsRegistered(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::NotificationService::AllBrowserContextsAndSources())) {
      registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
          content::NotificationService::AllBrowserContextsAndSources());
      registrar_.Add(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
          content::NotificationService::AllBrowserContextsAndSources());
    }
  }
}

void GoogleURLTracker::OnNavigationPending(
    const content::NotificationSource& navigation_controller_source,
    const content::NotificationSource& tab_contents_source,
    InfoBarTabHelper* infobar_helper,
    int pending_id) {
  InfoBarMap::iterator i(infobar_map_.find(infobar_helper));

  if (search_committed_) {
    search_committed_ = false;
    // Whether there's an existing infobar or not, we need to listen for the
    // load to commit, so we can show and/or update the infobar when it does.
    // (We may already be registered for this if there is an existing infobar
    // that had a previous pending search that hasn't yet committed.)
    if (!registrar_.IsRegistered(this,
                                 content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                                 navigation_controller_source)) {
      registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                      navigation_controller_source);
    }
    if (i == infobar_map_.end()) {
      // This is a search on a tab that doesn't have one of our infobars, so add
      // one.  Note that we only listen for the tab's destruction on this path;
      // if there was already an infobar, then either it's not yet showing and
      // we're already registered for this, or it is showing and its owner will
      // handle tearing it down when the tab is destroyed.
      registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                     tab_contents_source);
      infobar_map_.insert(std::make_pair(infobar_helper, MapEntry(
          (*infobar_creator_)(infobar_helper, this, fetched_google_url_),
          navigation_controller_source, tab_contents_source)));
    } else {
      // This is a new search on a tab where we already have an infobar (which
      // may or may not be showing).
      i->second.infobar->set_pending_id(pending_id);
    }
  } else if (i != infobar_map_.end()){
    if (i->second.infobar->showing()) {
      // This is a non-search navigation on a tab with a visible infobar.  If
      // there was a previous pending search on this tab, this means it won't
      // commit, so undo anything we did in response to seeing that.  Note that
      // if there was no pending search on this tab, these statements are
      // effectively a no-op.
      //
      // If this navigation actually commits, that will trigger the infobar's
      // owner to expire the infobar if need be.  If it doesn't commit, then
      // simply leaving the infobar as-is will have been the right thing.
      UnregisterForEntrySpecificNotifications(i->second, false);
      i->second.infobar->set_pending_id(0);
    } else {
      // Non-search navigation on a tab with a not-yet-shown infobar.  This
      // means the original search won't commit, so close the infobar.
      i->second.infobar->Close(false);
    }
  } else {
    // Non-search navigation on a tab without one of our infobars.  This is
    // irrelevant to us.
  }
}

void GoogleURLTracker::OnNavigationCommittedOrTabClosed(
    const InfoBarTabHelper* infobar_helper,
    const GURL& search_url) {
  InfoBarMap::iterator i(infobar_map_.find(infobar_helper));
  DCHECK(i != infobar_map_.end());
  const MapEntry& map_entry = i->second;

  if (!search_url.is_valid()) {
    // Tab closed, or we somehow tried to navigate to an invalid URL (?!).
    // InfoBarClosed() will take care of unregistering the notifications for
    // this tab.
    map_entry.infobar->Close(false);
    return;
  }

  // We're getting called because of a commit notification, so pass true for
  // |must_be_listening_for_commit|.
  UnregisterForEntrySpecificNotifications(map_entry, true);
  map_entry.infobar->Show(search_url);
}

void GoogleURLTracker::OnInstantCommitted(
    const content::NotificationSource& navigation_controller_source,
    const content::NotificationSource& tab_contents_source,
    InfoBarTabHelper* infobar_helper,
    const GURL& search_url) {
  // If this was the search we were listening for, OnNavigationPending() should
  // ensure we're registered for NAV_ENTRY_COMMITTED, and we should call
  // OnNavigationCommittedOrTabClosed() to simulate that firing.  Otherwise,
  // this is some sort of non-search navigation, so while we should still call
  // OnNavigationPending(), that function should then ensure that we're not
  // listening for NAV_ENTRY_COMMITTED on this tab, and we should not call
  // OnNavigationCommittedOrTabClosed() afterwards.  Note that we need to save
  // off |search_committed_| here because OnNavigationPending() will reset it.
  bool was_search_committed = search_committed_;
  OnNavigationPending(navigation_controller_source, tab_contents_source,
                      infobar_helper, 0);
  InfoBarMap::iterator i(infobar_map_.find(infobar_helper));
  DCHECK_EQ(was_search_committed, (i != infobar_map_.end()) &&
      registrar_.IsRegistered(this,
          content::NOTIFICATION_NAV_ENTRY_COMMITTED,
          i->second.navigation_controller_source));
  if (was_search_committed)
    OnNavigationCommittedOrTabClosed(infobar_helper, search_url);
}

void GoogleURLTracker::CloseAllInfoBars(bool redo_searches) {
  // Close all infobars, whether they've been added to tabs or not.
  while (!infobar_map_.empty())
    infobar_map_.begin()->second.infobar->Close(redo_searches);
}

void GoogleURLTracker::UnregisterForEntrySpecificNotifications(
    const MapEntry& map_entry,
    bool must_be_listening_for_commit) {
  // For tabs with non-showing infobars, we should always be listening for both
  // these notifications.  For tabs with showing infobars, we may be listening
  // for NOTIFICATION_NAV_ENTRY_COMMITTED if the user has performed a new search
  // on this tab.
  if (registrar_.IsRegistered(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                              map_entry.navigation_controller_source)) {
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                      map_entry.navigation_controller_source);
  } else {
    DCHECK(!must_be_listening_for_commit);
    DCHECK(map_entry.infobar->showing());
  }
  const bool registered_for_tab_contents_destroyed =
      registrar_.IsRegistered(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                              map_entry.tab_contents_source);
  DCHECK_NE(registered_for_tab_contents_destroyed,
            map_entry.infobar->showing());
  if (registered_for_tab_contents_destroyed) {
    registrar_.Remove(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                      map_entry.tab_contents_source);
  }

  // Our global listeners for these other notifications should be in place iff
  // we have any infobars still listening for commits.  These infobars are
  // either not yet shown or have received a new pending search atop an existing
  // infobar; in either case we want to catch subsequent pending non-search
  // navigations. See the various cases inside OnNavigationPending().
  for (InfoBarMap::const_iterator i(infobar_map_.begin());
       i != infobar_map_.end(); ++i) {
    if (registrar_.IsRegistered(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                                i->second.navigation_controller_source)) {
      DCHECK(registrar_.IsRegistered(this,
          content::NOTIFICATION_NAV_ENTRY_PENDING,
          content::NotificationService::AllBrowserContextsAndSources()));
      return;
    }
  }
  if (registrar_.IsRegistered(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::NotificationService::AllBrowserContextsAndSources())) {
    DCHECK(!search_committed_);
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::NotificationService::AllBrowserContextsAndSources());
    registrar_.Remove(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::NotificationService::AllBrowserContextsAndSources());
  }
}
