// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_infobar_delegate.h"

#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"


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
  DCHECK_EQ(net::StripWWWFromHost(new_google_url_),
            net::StripWWWFromHost(new_google_url));
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
  return l10n_util::GetStringFUTF16(
      IDS_GOOGLE_URL_TRACKER_INFOBAR_MESSAGE,
      net::StripWWWFromHost(new_google_url_),
      net::StripWWWFromHost(google_url_tracker_->google_url_));
}

string16 GoogleURLTrackerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  bool new_host = (button == BUTTON_OK);
  return l10n_util::GetStringFUTF16(
      new_host ?
          IDS_GOOGLE_URL_TRACKER_INFOBAR_SWITCH :
          IDS_GOOGLE_URL_TRACKER_INFOBAR_DONT_SWITCH,
      net::StripWWWFromHost(new_host ?
          new_google_url_ : google_url_tracker_->google_url_));
}
