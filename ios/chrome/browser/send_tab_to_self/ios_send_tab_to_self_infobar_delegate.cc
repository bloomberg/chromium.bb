// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/ios_send_tab_to_self_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_metrics.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace send_tab_to_self {

// static
std::unique_ptr<IOSSendTabToSelfInfoBarDelegate>
IOSSendTabToSelfInfoBarDelegate::Create(const SendTabToSelfEntry* entry) {
  return base::WrapUnique(new IOSSendTabToSelfInfoBarDelegate(entry));
}

IOSSendTabToSelfInfoBarDelegate::~IOSSendTabToSelfInfoBarDelegate() {}

IOSSendTabToSelfInfoBarDelegate::IOSSendTabToSelfInfoBarDelegate(
    const SendTabToSelfEntry* entry) {
  DCHECK(entry);
  entry_ = entry;
  RecordNotificationHistogram(SendTabToSelfNotification::kShown);
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSSendTabToSelfInfoBarDelegate::GetIdentifier() const {
  return SEND_TAB_TO_SELF_INFOBAR_DELEGATE;
}

int IOSSendTabToSelfInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

int IOSSendTabToSelfInfoBarDelegate::GetIconId() const {
  return IDR_IOS_INFOBAR_SEND_TAB_TO_SELF;
}

void IOSSendTabToSelfInfoBarDelegate::InfoBarDismissed() {
  Cancel();
}

base::string16 IOSSendTabToSelfInfoBarDelegate::GetMessageText() const {
  // The iOS confirm infobar controller requires the message to also include the
  // link text.
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE) +
         base::UTF8ToUTF16(" ") +
         l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE_URL);
}

base::string16 IOSSendTabToSelfInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE_URL);
}

GURL IOSSendTabToSelfInfoBarDelegate::GetLinkURL() const {
  return entry_->GetURL();
}

bool IOSSendTabToSelfInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  RecordNotificationHistogram(SendTabToSelfNotification::kOpened);
  infobar()->owner()->OpenURL(GetLinkURL(), disposition);
  return true;
}

bool IOSSendTabToSelfInfoBarDelegate::Cancel() {
  RecordNotificationHistogram(SendTabToSelfNotification::kDismissed);
  return true;
}

}  // namespace send_tab_to_self
