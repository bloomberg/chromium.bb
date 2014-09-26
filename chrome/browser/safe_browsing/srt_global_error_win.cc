// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_global_error_win.h"

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// The download link of the Software Removal Tool.
// TODO(mad): Should we only have the bubble show up on official Chrome build?
const char kSRTDownloadURL[] = "https://www.google.com/chrome/srt/";

// Enum values for the SRTPrompt histogram. Don't change order, always add
// to the end, before SRT_PROMPT_MAX, of course.
enum SRTPromptHistogramValue {
  SRT_PROMPT_SHOWN = 0,
  SRT_PROMPT_ACCEPTED = 1,
  SRT_PROMPT_DENIED = 2,

  SRT_PROMPT_MAX,
};

void RecordSRTPromptHistogram(SRTPromptHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "SoftwareReporter.PromptUsage", value, SRT_PROMPT_MAX);
}

}  // namespace

// SRTGlobalError ------------------------------------------------------------

SRTGlobalError::SRTGlobalError(GlobalErrorService* global_error_service)
    : global_error_service_(global_error_service) {
  DCHECK(global_error_service_);
}

SRTGlobalError::~SRTGlobalError() {
}

bool SRTGlobalError::HasMenuItem() {
  return true;
}

int SRTGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SRT_BUBBLE;
}

base::string16 SRTGlobalError::MenuItemLabel() {
  return l10n_util::GetStringUTF16(IDS_SRT_MENU_ITEM);
}

void SRTGlobalError::ExecuteMenuItem(Browser* browser) {
  // The menu item should never get executed while the bubble is shown, unless
  // we eventually change it to NOT close on deactivate.
  DCHECK(ShouldCloseOnDeactivate());
  DCHECK(GetBubbleView() == NULL);
  ShowBubbleView(browser);
}

void SRTGlobalError::ShowBubbleView(Browser* browser) {
  RecordSRTPromptHistogram(SRT_PROMPT_SHOWN);
  GlobalErrorWithStandardBubble::ShowBubbleView(browser);
}

base::string16 SRTGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_TITLE);
}

std::vector<base::string16> SRTGlobalError::GetBubbleViewMessages() {
  std::vector<base::string16> messages;
  messages.push_back(l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_TEXT));
  return messages;
}

base::string16 SRTGlobalError::GetBubbleViewAcceptButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_SRT_BUBBLE_DOWNLOAD_BUTTON_TEXT);
}

base::string16 SRTGlobalError::GetBubbleViewCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_NO_THANKS);
}

void SRTGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void SRTGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  RecordSRTPromptHistogram(SRT_PROMPT_ACCEPTED);
  browser->OpenURL(content::OpenURLParams(GURL(kSRTDownloadURL),
                                          content::Referrer(),
                                          NEW_FOREGROUND_TAB,
                                          ui::PAGE_TRANSITION_LINK,
                                          false));
  DismissGlobalError();
}

void SRTGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  RecordSRTPromptHistogram(SRT_PROMPT_DENIED);
  DismissGlobalError();
}

void SRTGlobalError::DismissGlobalError() {
  global_error_service_->RemoveGlobalError(this);
  delete this;
}
