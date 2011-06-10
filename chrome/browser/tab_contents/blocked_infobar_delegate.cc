// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/blocked_infobar_delegate.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

BlockedInfoBarDelegate::BlockedInfoBarDelegate(TabContentsWrapper* wrapper,
                                               int message_resource_id,
                                               int button_resource_id,
                                               const GURL& url)
    : ConfirmInfoBarDelegate(wrapper->tab_contents()),
      wrapper_(wrapper),
      message_resource_id_(message_resource_id),
      button_resource_id_(button_resource_id),
      url_(url) {}

BlockedInfoBarDelegate* BlockedInfoBarDelegate::AsBlockedInfoBarDelegate() {
  return this;
}

string16 BlockedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(message_resource_id_);
}

int BlockedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 BlockedInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  DCHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(button_resource_id_);
};

string16 BlockedInfoBarDelegate::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool BlockedInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  TabContents* contents = wrapper_->tab_contents();
  contents->OpenURL(url_, GURL(), disposition, PageTransition::LINK);
  return false;
}

BlockedDisplayingInfoBarDelegate::BlockedDisplayingInfoBarDelegate(
    TabContentsWrapper* wrapper)
    : BlockedInfoBarDelegate(
        wrapper,
        IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT,
        IDS_ALLOW_INSECURE_CONTENT_BUTTON,
        GURL("http://www.google.com?q=blocked+display")) {
}

bool BlockedDisplayingInfoBarDelegate::Accept() {
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
        GURL("http://www.google.com?q=blocked+running")) {
}

BlockedRunningInfoBarDelegate*
BlockedRunningInfoBarDelegate::AsBlockedRunningInfoBarDelegate() {
  return this;
}

bool BlockedRunningInfoBarDelegate::Accept() {
  wrapper()->Send(new ViewMsg_SetAllowRunningInsecureContent(
      wrapper()->routing_id(), true));
  return true;
}

