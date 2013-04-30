// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/active_tab_tracker.h"

#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

namespace {

// Amount of time a page has to be active before we commit it.
const int kTimeBeforeCommitMS = 1000;

// Number of seconds input should be received before considered idle.
const int kIdleTimeSeconds = 30;

}  // namespace

#if !defined(OS_WIN) && !defined(USE_AURA)
// static
NativeFocusTracker* NativeFocusTracker::Create(NativeFocusTrackerHost* host) {
  return NULL;
}
#endif

ActiveTabTracker::ActiveTabTracker()
    : browser_(NULL),
      web_contents_(NULL),
      idle_state_(IDLE_STATE_UNKNOWN),
      timer_(false, false),
      weak_ptr_factory_(this) {
  native_focus_tracker_.reset(NativeFocusTracker::Create(this));
  Browser* browser =
      BrowserList::GetInstance(chrome::GetActiveDesktop())->GetLastActive();
  SetBrowser(browser);
  BrowserList::AddObserver(this);
}

ActiveTabTracker::~ActiveTabTracker() {
  native_focus_tracker_.reset();
  SetBrowser(NULL);
  BrowserList::RemoveObserver(this);
}

void ActiveTabTracker::ActiveTabChanged(content::WebContents* old_contents,
                                        content::WebContents* new_contents,
                                        int index,
                                        int reason) {
  SetWebContents(new_contents);
}

void ActiveTabTracker::TabReplacedAt(TabStripModel* tab_strip_model,
                                     content::WebContents* old_contents,
                                     content::WebContents* new_contents,
                                     int index) {
  if (index == tab_strip_model->selection_model().active())
    SetWebContents(new_contents);
}

void ActiveTabTracker::TabStripEmpty() {
  SetBrowser(NULL);
}

void ActiveTabTracker::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    SetBrowser(NULL);
}

void ActiveTabTracker::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  const GURL url = GetURLFromWebContents();
  if (url == url_)
    return;

  CommitActiveTime();
  url_ = url;
}

void ActiveTabTracker::SetBrowser(Browser* browser) {
  if (browser_ == browser)
    return;

  CommitActiveTime();
  if (browser_)
    browser_->tab_strip_model()->RemoveObserver(this);
  // Don't track anything for otr profiles.
  if (browser && browser->profile()->IsOffTheRecord())
    browser = NULL;
  browser_ = browser;
  content::WebContents* web_contents = NULL;
  if (browser_) {
    TabStripModel* tab_strip = browser_->tab_strip_model();
    tab_strip->AddObserver(this);
    web_contents = tab_strip->GetActiveWebContents();
  } else {
    idle_state_ = IDLE_STATE_UNKNOWN;
    timer_.Stop();
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
  SetWebContents(web_contents);
}

void ActiveTabTracker::SetWebContents(content::WebContents* web_contents) {
  if (web_contents_ == web_contents)
    return;

  CommitActiveTime();

  active_time_ = base::TimeTicks::Now();
  web_contents_ = web_contents;
  url_ = GetURLFromWebContents();
  registrar_.RemoveAll();
  // TODO(sky): this isn't quite right. We should really not include transient
  // entries here. For that we need to make Browser::NavigationStateChanged()
  // call through to this class.
  if (web_contents_) {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::Source<content::NavigationController>(
                       &web_contents_->GetController()));
    QueryIdleState();
  }
}

void ActiveTabTracker::SetIdleState(IdleState idle_state) {
  if (idle_state_ != idle_state) {
    if (idle_state_ == IDLE_STATE_ACTIVE)
      CommitActiveTime();
    else if (idle_state == IDLE_STATE_ACTIVE)
      active_time_ = base::TimeTicks::Now();
    idle_state_ = idle_state;
  }
  if (browser_) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kIdleTimeSeconds),
                 base::Bind(&ActiveTabTracker::QueryIdleState,
                            base::Unretained(this)));
  }
}

void ActiveTabTracker::QueryIdleState() {
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  CalculateIdleState(kIdleTimeSeconds,
                     base::Bind(&ActiveTabTracker::SetIdleState,
                                weak_ptr_factory_.GetWeakPtr()));
}

GURL ActiveTabTracker::GetURLFromWebContents() const {
  if (!web_contents_)
    return GURL();

  // TODO: handle subframe transitions better. Maybe go back to first entry
  // that isn't a main frame?
  content::NavigationEntry* entry =
      web_contents_->GetController().GetLastCommittedEntry();
  if (!entry || !PageTransitionIsMainFrame(entry->GetTransitionType()))
    return GURL();
  return !entry->GetUserTypedURL().is_empty() ?
      entry->GetUserTypedURL() : entry->GetURL();
}

void ActiveTabTracker::CommitActiveTime() {
  const base::TimeDelta active_delta = base::TimeTicks::Now() - active_time_;
  active_time_ = base::TimeTicks::Now();

  if (!web_contents_ || url_.is_empty() || idle_state_ != IDLE_STATE_ACTIVE ||
      active_delta.InMilliseconds() < kTimeBeforeCommitMS)
    return;

  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  if (history)
    history->IncreaseSegmentDuration(url_, base::Time::Now(), active_delta);
}
