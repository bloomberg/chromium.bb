// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/blocked_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum BlockedInfoBarEvent {
  BLOCKED_INFOBAR_EVENT_SHOWN = 0,      // Infobar was added to the tab.
  BLOCKED_INFOBAR_EVENT_ALLOWED,        // User clicked allowed anyay button.
  BLOCKED_INFOBAR_EVENT_CANCELLED,      // Reserved (if future cancel button).
  BLOCKED_INFOBAR_EVENT_DISMISSED,      // User clicked "x" to dismiss bar.
  BLOCKED_INFOBAR_EVENT_LAST            // First unused value.
};

const char kDisplayingLearnMoreURL[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=1342710";

const char kRunningLearnMoreURL[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=1342714";

}  // namespace

BlockedInfoBarDelegate::BlockedInfoBarDelegate(TabContentsWrapper* wrapper,
                                               int message_resource_id,
                                               int button_resource_id,
                                               const GURL& url)
    : ConfirmInfoBarDelegate(wrapper->tab_contents()),
      wrapper_(wrapper),
      message_resource_id_(message_resource_id),
      button_resource_id_(button_resource_id),
      url_(url) {
}

BlockedInfoBarDelegate* BlockedInfoBarDelegate::AsBlockedInfoBarDelegate() {
  return this;
}

string16 BlockedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(message_resource_id_);
}

int BlockedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 BlockedInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      button == BUTTON_OK ? IDS_OK : button_resource_id_);
};

string16 BlockedInfoBarDelegate::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool BlockedInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  TabContents* contents = wrapper_->tab_contents();
  if (disposition == CURRENT_TAB)
    disposition = NEW_FOREGROUND_TAB;
  contents->OpenURL(url_, GURL(), disposition, PageTransition::LINK);
  return false;
}

BlockedRunningInfoBarDelegate*
BlockedInfoBarDelegate::AsBlockedRunningInfoBarDelegate() {
  return NULL;
}

BlockedDisplayingInfoBarDelegate::BlockedDisplayingInfoBarDelegate(
    TabContentsWrapper* wrapper)
    : BlockedInfoBarDelegate(
        wrapper,
        IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT,
        IDS_ALLOW_INSECURE_CONTENT_BUTTON,
        GURL(kDisplayingLearnMoreURL)) {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.DisplayingInfoBar",
                            BLOCKED_INFOBAR_EVENT_SHOWN,
                            BLOCKED_INFOBAR_EVENT_LAST);
}

void BlockedDisplayingInfoBarDelegate::InfoBarDismissed() {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.DisplayingInfoBar",
                            BLOCKED_INFOBAR_EVENT_DISMISSED,
                            BLOCKED_INFOBAR_EVENT_LAST);
  InfoBarDelegate::InfoBarDismissed();
}

bool BlockedDisplayingInfoBarDelegate::Accept() {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.DisplayingInfoBar",
                            BLOCKED_INFOBAR_EVENT_CANCELLED,
                            BLOCKED_INFOBAR_EVENT_LAST);
  return true;
}

bool BlockedDisplayingInfoBarDelegate::Cancel() {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.DisplayingInfoBar",
                            BLOCKED_INFOBAR_EVENT_ALLOWED,
                            BLOCKED_INFOBAR_EVENT_LAST);
  wrapper()->Send(new ViewMsg_SetAllowDisplayingInsecureContent(
      wrapper()->routing_id(), true));
  return true;
}

BlockedRunningInfoBarDelegate::BlockedRunningInfoBarDelegate(
    TabContentsWrapper* wrapper)
    : BlockedInfoBarDelegate(
        wrapper,
        IDS_BLOCKED_RUNNING_INSECURE_CONTENT,
        IDS_ALLOW_INSECURE_CONTENT_BUTTON,
        GURL(kRunningLearnMoreURL)) {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.RunningInfoBar",
                            BLOCKED_INFOBAR_EVENT_SHOWN,
                            BLOCKED_INFOBAR_EVENT_LAST);
}

BlockedRunningInfoBarDelegate*
BlockedRunningInfoBarDelegate::AsBlockedRunningInfoBarDelegate() {
  return this;
}

void BlockedRunningInfoBarDelegate::InfoBarDismissed() {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.RunningInfoBar",
                            BLOCKED_INFOBAR_EVENT_DISMISSED,
                            BLOCKED_INFOBAR_EVENT_LAST);
  InfoBarDelegate::InfoBarDismissed();
}

bool BlockedRunningInfoBarDelegate::Accept() {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.RunningInfoBar",
                            BLOCKED_INFOBAR_EVENT_CANCELLED,
                            BLOCKED_INFOBAR_EVENT_LAST);
  return true;
}

bool BlockedRunningInfoBarDelegate::Cancel() {
  UMA_HISTOGRAM_ENUMERATION("MixedContent.RunningInfoBar",
                            BLOCKED_INFOBAR_EVENT_ALLOWED,
                            BLOCKED_INFOBAR_EVENT_LAST);
  wrapper()->Send(new ViewMsg_SetAllowRunningInsecureContent(
      wrapper()->routing_id(), true));
  return true;
}


