// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/insecure_content_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;

// static
void InsecureContentInfoBarDelegate::Create(InfoBarService* infobar_service,
                                            InfoBarType type) {
  scoped_ptr<InfoBarDelegate> new_infobar(
      new InsecureContentInfoBarDelegate(infobar_service, type));

  // Only supsersede an existing insecure content infobar if we are upgrading
  // from DISPLAY to RUN.
  for (size_t i = 0; i < infobar_service->GetInfoBarCount(); ++i) {
    InsecureContentInfoBarDelegate* delegate = infobar_service->
        GetInfoBarDelegateAt(i)->AsInsecureContentInfoBarDelegate();
    if (delegate != NULL) {
      if ((type == RUN) && (delegate->type_ == DISPLAY))
        return;
      infobar_service->ReplaceInfoBar(delegate, new_infobar.Pass());
      break;
    }
  }
  if (new_infobar.get())
    infobar_service->AddInfoBar(new_infobar.Pass());

  UMA_HISTOGRAM_ENUMERATION("InsecureContentInfoBarDelegateV2",
      (type == DISPLAY) ? DISPLAY_INFOBAR_SHOWN : RUN_INFOBAR_SHOWN,
      NUM_EVENTS);
}

InsecureContentInfoBarDelegate::InsecureContentInfoBarDelegate(
    InfoBarService* infobar_service,
    InfoBarType type)
    : ConfirmInfoBarDelegate(infobar_service),
      type_(type) {
}

InsecureContentInfoBarDelegate::~InsecureContentInfoBarDelegate() {
}

void InsecureContentInfoBarDelegate::InfoBarDismissed() {
  UMA_HISTOGRAM_ENUMERATION("InsecureContentInfoBarDelegateV2",
      (type_ == DISPLAY) ? DISPLAY_INFOBAR_DISMISSED : RUN_INFOBAR_DISMISSED,
      NUM_EVENTS);
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

InsecureContentInfoBarDelegate*
    InsecureContentInfoBarDelegate::AsInsecureContentInfoBarDelegate() {
  return this;
}

string16 InsecureContentInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT);
}

string16 InsecureContentInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(button == BUTTON_OK ?
      IDS_BLOCK_INSECURE_CONTENT_BUTTON : IDS_ALLOW_INSECURE_CONTENT_BUTTON);
}

// OK button is labelled "don't load".  It triggers Accept(), but really
// means stay secure, so do nothing but count the event and dismiss.
bool InsecureContentInfoBarDelegate::Accept() {
  UMA_HISTOGRAM_ENUMERATION("InsecureContentInfoBarDelegateV2",
      (type_ == DISPLAY) ? DISPLAY_USER_DID_NOT_LOAD : RUN_USER_DID_NOT_LOAD,
      NUM_EVENTS);
  return true;
}


// Cancel button is labelled "load anyways".  It triggers Cancel(), but really
// means become insecure, so do the work of reloading the page.
bool InsecureContentInfoBarDelegate::Cancel() {
  UMA_HISTOGRAM_ENUMERATION("InsecureContentInfoBarDelegateV2",
      (type_ == DISPLAY) ? DISPLAY_USER_OVERRIDE : RUN_USER_OVERRIDE,
      NUM_EVENTS);

  content::WebContents* web_contents = owner()->GetWebContents();
  if (web_contents) {
    int32 routing_id = web_contents->GetRoutingID();
    web_contents->Send((type_ == DISPLAY) ?
                       static_cast<IPC::Message*>(
                           new ChromeViewMsg_SetAllowDisplayingInsecureContent(
                               routing_id, true)) :
                       new ChromeViewMsg_SetAllowRunningInsecureContent(
                           routing_id, true));
  }
  return true;
}

string16 InsecureContentInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool InsecureContentInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  owner()->GetWebContents()->OpenURL(OpenURLParams(
      google_util::AppendGoogleLocaleParam(GURL(
      "https://www.google.com/support/chrome/bin/answer.py?answer=1342714")),
      content::Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false));
  return false;
}
