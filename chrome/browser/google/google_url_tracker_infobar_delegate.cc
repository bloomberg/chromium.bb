// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_infobar_delegate.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"


// static
GoogleURLTrackerInfoBarDelegate* GoogleURLTrackerInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    GoogleURLTracker* google_url_tracker,
    const GURL& search_url) {
  return static_cast<GoogleURLTrackerInfoBarDelegate*>(
      infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
          new GoogleURLTrackerInfoBarDelegate(
              infobar_service, google_url_tracker, search_url))));
}

bool GoogleURLTrackerInfoBarDelegate::Accept() {
  google_url_tracker_->AcceptGoogleURL(true);
  return false;
}

bool GoogleURLTrackerInfoBarDelegate::Cancel() {
  google_url_tracker_->CancelGoogleURL();
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

void GoogleURLTrackerInfoBarDelegate::Update(const GURL& search_url) {
  StoreActiveEntryUniqueID();
  search_url_ = search_url;
  pending_id_ = 0;
}

void GoogleURLTrackerInfoBarDelegate::Close(bool redo_search) {
  if (redo_search) {
    // Re-do the user's search on the new domain.
    DCHECK(search_url_.is_valid());
    url_canon::Replacements<char> replacements;
    const std::string& host(google_url_tracker_->fetched_google_url().host());
    replacements.SetHost(host.data(), url_parse::Component(0, host.length()));
    GURL new_search_url(search_url_.ReplaceComponents(replacements));
    if (new_search_url.is_valid()) {
      owner()->GetWebContents()->OpenURL(content::OpenURLParams(
          new_search_url, content::Referrer(), CURRENT_TAB,
          content::PAGE_TRANSITION_GENERATED, false));
    }
  }

  owner()->RemoveInfoBar(this);
}

GoogleURLTrackerInfoBarDelegate::GoogleURLTrackerInfoBarDelegate(
    InfoBarService* infobar_service,
    GoogleURLTracker* google_url_tracker,
    const GURL& search_url)
    : ConfirmInfoBarDelegate(infobar_service),
      google_url_tracker_(google_url_tracker),
      search_url_(search_url),
      pending_id_(0) {
}

GoogleURLTrackerInfoBarDelegate::~GoogleURLTrackerInfoBarDelegate() {
}

string16 GoogleURLTrackerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_GOOGLE_URL_TRACKER_INFOBAR_MESSAGE,
      net::StripWWWFromHost(google_url_tracker_->fetched_google_url()),
      net::StripWWWFromHost(google_url_tracker_->google_url()));
}

string16 GoogleURLTrackerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringFUTF16(
        IDS_GOOGLE_URL_TRACKER_INFOBAR_SWITCH,
        net::StripWWWFromHost(google_url_tracker_->fetched_google_url()));
  }
  return l10n_util::GetStringFUTF16(
      IDS_GOOGLE_URL_TRACKER_INFOBAR_DONT_SWITCH,
      net::StripWWWFromHost(google_url_tracker_->google_url()));
}
