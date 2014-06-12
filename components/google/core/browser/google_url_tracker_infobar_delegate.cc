// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_url_tracker_infobar_delegate.h"

#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/browser/google_url_tracker_navigation_helper.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "grit/components_strings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"


// static
infobars::InfoBar* GoogleURLTrackerInfoBarDelegate::Create(
    infobars::InfoBarManager* infobar_manager,
    GoogleURLTracker* google_url_tracker,
    const GURL& search_url) {
  return infobar_manager->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new GoogleURLTrackerInfoBarDelegate(
          google_url_tracker, search_url))));
}

bool GoogleURLTrackerInfoBarDelegate::Accept() {
  google_url_tracker_->AcceptGoogleURL(true);
  return false;
}

bool GoogleURLTrackerInfoBarDelegate::Cancel() {
  google_url_tracker_->CancelGoogleURL();
  return false;
}

void GoogleURLTrackerInfoBarDelegate::Update(const GURL& search_url) {
  StoreActiveEntryUniqueID();
  search_url_ = search_url;
  pending_id_ = 0;
}

void GoogleURLTrackerInfoBarDelegate::Close(bool redo_search) {
  // Calling OpenURL() will auto-close us asynchronously.  It's easier for
  // various classes (e.g. GoogleURLTrackerMapEntry) to reason about things if
  // the closure always happens synchronously, so we always call RemoveInfoBar()
  // directly, then OpenURL() if desirable.  (This calling order is safer if
  // for some reason in the future OpenURL() were to close us synchronously.)
  GURL new_search_url;
  if (redo_search) {
    // Re-do the user's search on the new domain.
    DCHECK(search_url_.is_valid());
    url::Replacements<char> replacements;
    const std::string& host(google_url_tracker_->fetched_google_url().host());
    replacements.SetHost(host.data(), url::Component(0, host.length()));
    new_search_url = search_url_.ReplaceComponents(replacements);
  }

  // Take ownership of |navigation_helper_| in order to ensure that it stays
  // alive for the duration of this method.
  scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper =
      navigation_helper_.Pass();
  infobar()->RemoveSelf();
  // WARNING: |this| may be deleted at this point!  Do not access any members!

  if (new_search_url.is_valid())
    navigation_helper->OpenURL(new_search_url, CURRENT_TAB, false);
}

GoogleURLTrackerInfoBarDelegate::GoogleURLTrackerInfoBarDelegate(
    GoogleURLTracker* google_url_tracker,
    const GURL& search_url)
    : ConfirmInfoBarDelegate(),
      google_url_tracker_(google_url_tracker),
      navigation_helper_weak_ptr_(NULL),
      search_url_(search_url),
      pending_id_(0) {
}

GoogleURLTrackerInfoBarDelegate::~GoogleURLTrackerInfoBarDelegate() {
}

base::string16 GoogleURLTrackerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_GOOGLE_URL_TRACKER_INFOBAR_MESSAGE,
      net::StripWWWFromHost(google_url_tracker_->fetched_google_url()),
      net::StripWWWFromHost(google_url_tracker_->google_url()));
}

base::string16 GoogleURLTrackerInfoBarDelegate::GetButtonLabel(
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

base::string16 GoogleURLTrackerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_GOOGLE_URL_TRACKER_INFOBAR_LEARN_MORE);
}

bool GoogleURLTrackerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  navigation_helper_->OpenURL(
      GURL("https://www.google.com/support/chrome/bin/answer.py?"
           "answer=1618699"),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      true);
  return false;
}

bool GoogleURLTrackerInfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  return (details.entry_id != contents_unique_id()) &&
      (details.entry_id != pending_id_);
}
