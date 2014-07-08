// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_url_tracker.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_switches.h"
#include "components/google/core/browser/google_url_tracker_infobar_delegate.h"
#include "components/google/core/browser/google_url_tracker_navigation_helper.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"


const char GoogleURLTracker::kDefaultGoogleHomepage[] =
    "http://www.google.com/";
const char GoogleURLTracker::kSearchDomainCheckURL[] =
    "https://www.google.com/searchdomaincheck?format=url&type=chrome";

GoogleURLTracker::GoogleURLTracker(scoped_ptr<GoogleURLTrackerClient> client,
                                   Mode mode)
    : client_(client.Pass()),
      google_url_(mode == UNIT_TEST_MODE ?
          kDefaultGoogleHomepage :
          client_->GetPrefs()->GetString(prefs::kLastKnownGoogleURL)),
      fetcher_id_(0),
      in_startup_sleep_(true),
      already_fetched_(false),
      need_to_fetch_(false),
      need_to_prompt_(false),
      search_committed_(false),
      weak_ptr_factory_(this) {
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  client_->set_google_url_tracker(this);

  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay five seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  //
  // In UNIT_TEST_MODE, where we want to explicitly control when the tracker
  // "wakes up", we do nothing at all.
  if (mode == NORMAL_MODE) {
    static const int kStartFetchDelayMS = 5000;
    base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&GoogleURLTracker::FinishSleep,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kStartFetchDelayMS));
  }
}

GoogleURLTracker::~GoogleURLTracker() {
  // We should only reach here after any tabs and their infobars have been torn
  // down.
  DCHECK(entry_map_.empty());
}

void GoogleURLTracker::RequestServerCheck(bool force) {
  // If this instance already has a fetcher, SetNeedToFetch() is unnecessary,
  // and changing |already_fetched_| is wrong.
  if (!fetcher_) {
    if (force)
      already_fetched_ = false;
    SetNeedToFetch();
  }
}

void GoogleURLTracker::SearchCommitted() {
  if (need_to_prompt_) {
    search_committed_ = true;
    // These notifications will fire a bit later in the same call chain we're
    // currently in.
    if (!client_->IsListeningForNavigationStart())
      client_->SetListeningForNavigationStart(true);
  }
}

void GoogleURLTracker::AcceptGoogleURL(bool redo_searches) {
  GURL old_google_url = google_url_;
  google_url_ = fetched_google_url_;
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(prefs::kLastKnownGoogleURL, google_url_.spec());
  prefs->SetString(prefs::kLastPromptedGoogleURL, google_url_.spec());
  NotifyGoogleURLUpdated();

  need_to_prompt_ = false;
  CloseAllEntries(redo_searches);
}

void GoogleURLTracker::CancelGoogleURL() {
  client_->GetPrefs()->SetString(prefs::kLastPromptedGoogleURL,
                                 fetched_google_url_.spec());
  need_to_prompt_ = false;
  CloseAllEntries(false);
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
  base::TrimWhitespace(url_str, base::TRIM_ALL, &url_str);
  GURL url(url_str);
  if (!url.is_valid() || (url.path().length() > 1) || url.has_query() ||
      url.has_ref() ||
      !google_util::IsGoogleDomainUrl(url,
                                      google_util::DISALLOW_SUBDOMAIN,
                                      google_util::DISALLOW_NON_STANDARD_PORTS))
    return;

  std::swap(url, fetched_google_url_);
  GURL last_prompted_url(
      client_->GetPrefs()->GetString(prefs::kLastPromptedGoogleURL));

  if (last_prompted_url.is_empty()) {
    // On the very first run of Chrome, when we've never looked up the URL at
    // all, we should just silently switch over to whatever we get immediately.
    AcceptGoogleURL(true);  // Arg is irrelevant.
    return;
  }

  base::string16 fetched_host(net::StripWWWFromHost(fetched_google_url_));
  if (fetched_google_url_ == google_url_) {
    // Either the user has continually been on this URL, or we prompted for a
    // different URL but have now changed back before they responded to any of
    // the prompts.  In this latter case we want to close any infobars and stop
    // prompting.
    CancelGoogleURL();
  } else if (fetched_host == net::StripWWWFromHost(google_url_)) {
    // Similar to the above case, but this time the new URL differs from the
    // existing one, probably due to switching between HTTP and HTTPS searching.
    // Like before we want to close any infobars and stop prompting; we also
    // want to silently accept the change in scheme.  We don't redo open
    // searches so as to avoid suddenly changing a page the user might be
    // interacting with; it's enough to simply get future searches right.
    AcceptGoogleURL(false);
  } else if (fetched_host == net::StripWWWFromHost(last_prompted_url)) {
    // We've re-fetched a TLD the user previously turned down.  Although the new
    // URL might have a different scheme than the old, we want to preserve the
    // user's decision.  Note that it's possible that, like in the above two
    // cases, we fetched yet another different URL in the meantime, which we
    // have infobars prompting about; in this case, as in those above, we want
    // to go ahead and close the infobars and stop prompting, since we've
    // switched back away from that URL.
    CancelGoogleURL();
  } else {
    // We've fetched a URL with a different TLD than the user is currently using
    // or was previously prompted about.  This means we need to prompt again.
    need_to_prompt_ = true;

    // As in all the above cases, there could be infobars prompting about some
    // URL.  If these URLs have the same TLD (e.g. for scheme changes), we can
    // simply leave the existing infobars open as their messages will still be
    // accurate.  Otherwise we go ahead and close them because we need to
    // display a new message.
    // Note: |url| is the previous |fetched_google_url_|.
    if (url.is_valid() && (fetched_host != net::StripWWWFromHost(url)))
      CloseAllEntries(false);
  }
}

void GoogleURLTracker::OnIPAddressChanged() {
  already_fetched_ = false;
  StartFetchIfDesirable();
}

void GoogleURLTracker::Shutdown() {
  client_.reset();
  fetcher_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void GoogleURLTracker::DeleteMapEntryForManager(
    const infobars::InfoBarManager* infobar_manager) {
  // WARNING: |infobar_manager| may point to a deleted object.  Do not
  // dereference it!  See OnTabClosed().
  EntryMap::iterator i(entry_map_.find(infobar_manager));
  DCHECK(i != entry_map_.end());
  GoogleURLTrackerMapEntry* map_entry = i->second;

  UnregisterForEntrySpecificNotifications(map_entry, false);
  entry_map_.erase(i);
  delete map_entry;
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

  // Some switches should disable the Google URL tracker entirely.  If we can't
  // do background networking, we can't do the necessary fetch, and if the user
  // specified a Google base URL manually, we shouldn't bother to look up any
  // alternatives or offer to switch to them.
  if (!client_->IsBackgroundNetworkingEnabled() ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kGoogleBaseURL))
    return;

  already_fetched_ = true;
  fetcher_.reset(net::URLFetcher::Create(
      fetcher_id_, GURL(kSearchDomainCheckURL), net::URLFetcher::GET, this));
  ++fetcher_id_;
  // We don't want this fetch to set new entries in the cache or cookies, lest
  // we alarm the user.
  fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetRequestContext(client_->GetRequestContext());

  // Configure to max_retries at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher_->SetMaxRetriesOn5xx(kMaxRetries);

  fetcher_->Start();
}

void GoogleURLTracker::OnNavigationPending(
    scoped_ptr<GoogleURLTrackerNavigationHelper> nav_helper,
    infobars::InfoBarManager* infobar_manager,
    int pending_id) {
  GoogleURLTrackerMapEntry* map_entry = NULL;

  EntryMap::iterator i(entry_map_.find(infobar_manager));
  if (i != entry_map_.end())
    map_entry = i->second;

  if (search_committed_) {
    search_committed_ = false;
    if (!map_entry) {
      // This is a search on a tab that doesn't have one of our infobars, so
      // prepare to add one.  Note that we only listen for the tab's destruction
      // on this path; if there was already a map entry, then either it doesn't
      // yet have an infobar and we're already registered for this, or it has an
      // infobar and the infobar's owner will handle tearing it down when the
      // tab is destroyed.
      map_entry = new GoogleURLTrackerMapEntry(
          this, infobar_manager, nav_helper.Pass());
      map_entry->navigation_helper()->SetListeningForTabDestruction(true);
      entry_map_.insert(std::make_pair(infobar_manager, map_entry));
    } else if (map_entry->infobar_delegate()) {
      // This is a new search on a tab where we already have an infobar.
      map_entry->infobar_delegate()->set_pending_id(pending_id);
    }

    // Whether there's an existing infobar or not, we need to listen for the
    // load to commit, so we can show and/or update the infobar when it does.
    // (We may already be registered for this if there is an existing infobar
    // that had a previous pending search that hasn't yet committed.)
    if (!map_entry->navigation_helper()->IsListeningForNavigationCommit())
      map_entry->navigation_helper()->SetListeningForNavigationCommit(true);
  } else if (map_entry) {
    if (map_entry->has_infobar_delegate()) {
      // This is a non-search navigation on a tab with an infobar.  If there was
      // a previous pending search on this tab, this means it won't commit, so
      // undo anything we did in response to seeing that.  Note that if there
      // was no pending search on this tab, these statements are effectively a
      // no-op.
      //
      // If this navigation actually commits, that will trigger the infobar's
      // owner to expire the infobar if need be.  If it doesn't commit, then
      // simply leaving the infobar as-is will have been the right thing.
      UnregisterForEntrySpecificNotifications(map_entry, false);
      map_entry->infobar_delegate()->set_pending_id(0);
    } else {
      // Non-search navigation on a tab with an entry that has not yet created
      // an infobar.  This means the original search won't commit, so delete the
      // entry.
      map_entry->Close(false);
    }
  } else {
    // Non-search navigation on a tab without an infobars.  This is irrelevant
    // to us.
  }
}

void GoogleURLTracker::OnNavigationCommitted(
    infobars::InfoBarManager* infobar_manager,
    const GURL& search_url) {
  EntryMap::iterator i(entry_map_.find(infobar_manager));
  DCHECK(i != entry_map_.end());
  GoogleURLTrackerMapEntry* map_entry = i->second;
  DCHECK(search_url.is_valid());

  UnregisterForEntrySpecificNotifications(map_entry, true);
  if (map_entry->has_infobar_delegate()) {
    map_entry->infobar_delegate()->Update(search_url);
  } else {
    infobars::InfoBar* infobar = GoogleURLTrackerInfoBarDelegate::Create(
        infobar_manager, this, search_url);
    if (infobar) {
      map_entry->SetInfoBarDelegate(
          static_cast<GoogleURLTrackerInfoBarDelegate*>(infobar->delegate()));
    } else {
      map_entry->Close(false);
    }
  }
}

void GoogleURLTracker::OnTabClosed(
    GoogleURLTrackerNavigationHelper* nav_helper) {
  // Because InfoBarManager tears itself down on tab destruction, it's possible
  // to get a non-NULL InfoBarManager pointer here, depending on which order
  // notifications fired in.  Likewise, the pointer in |entry_map_| (and in its
  // associated MapEntry) may point to deleted memory.  Therefore, if we were
  // to access the InfoBarManager* we have for this tab, we'd need to ensure we
  // just looked at the raw pointer value, and never dereferenced it.  This
  // function doesn't need to do even that, but others in the call chain from
  // here might (and have comments pointing back here).
  for (EntryMap::iterator i(entry_map_.begin()); i != entry_map_.end(); ++i) {
    if (i->second->navigation_helper() == nav_helper) {
      i->second->Close(false);
      return;
    }
  }
  NOTREACHED();
}

scoped_ptr<GoogleURLTracker::Subscription> GoogleURLTracker::RegisterCallback(
    const OnGoogleURLUpdatedCallback& cb) {
  return callback_list_.Add(cb);
}

void GoogleURLTracker::CloseAllEntries(bool redo_searches) {
  // Delete all entries, whether they have infobars or not.
  while (!entry_map_.empty())
    entry_map_.begin()->second->Close(redo_searches);
}

void GoogleURLTracker::UnregisterForEntrySpecificNotifications(
    GoogleURLTrackerMapEntry* map_entry,
    bool must_be_listening_for_commit) {
  // For tabs with map entries but no infobars, we should always be listening
  // for both these notifications.  For tabs with infobars, we may be listening
  // for navigation commits if the user has performed a new search on this tab.
  if (map_entry->navigation_helper()->IsListeningForNavigationCommit()) {
    map_entry->navigation_helper()->SetListeningForNavigationCommit(false);
  } else {
    DCHECK(!must_be_listening_for_commit);
    DCHECK(map_entry->has_infobar_delegate());
  }
  const bool registered_for_tab_destruction =
      map_entry->navigation_helper()->IsListeningForTabDestruction();
  DCHECK_NE(registered_for_tab_destruction, map_entry->has_infobar_delegate());
  if (registered_for_tab_destruction) {
    map_entry->navigation_helper()->SetListeningForTabDestruction(false);
  }

  // Our global listeners for these other notifications should be in place iff
  // we have any tabs still listening for commits.  These tabs either have no
  // infobars or have received new pending searches atop existing infobars; in
  // either case we want to catch subsequent pending non-search navigations.
  // See the various cases inside OnNavigationPending().
  for (EntryMap::const_iterator i(entry_map_.begin()); i != entry_map_.end();
       ++i) {
    if (i->second->navigation_helper()->IsListeningForNavigationCommit()) {
      DCHECK(client_->IsListeningForNavigationStart());
      return;
    }
  }
  if (client_->IsListeningForNavigationStart()) {
    DCHECK(!search_committed_);
    client_->SetListeningForNavigationStart(false);
  }
}

void GoogleURLTracker::NotifyGoogleURLUpdated() {
  callback_list_.Notify();
}
