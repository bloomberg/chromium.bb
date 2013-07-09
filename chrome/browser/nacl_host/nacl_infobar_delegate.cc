// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

namespace {
// The URL for the "learn more" article.
const char kNaClLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=ib_nacl";

}  // namespace

// static
void NaClInfoBarDelegate::Create(int render_process_id, int render_view_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (!rvh)
    return;

  WebContents* wc = WebContents::FromRenderViewHost(rvh);
  if (!wc)
    return;

  InfoBarService* infobar_service = InfoBarService::FromWebContents(wc);
  if (infobar_service)
    infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
        new NaClInfoBarDelegate(wc, infobar_service)));
}

string16 NaClInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_NACL_APP_MISSING_ARCH_MESSAGE);
}

int NaClInfoBarDelegate::GetButtons() const { return BUTTON_NONE; }

string16 NaClInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool NaClInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  content::OpenURLParams params(
      GURL(kNaClLearnMoreUrl), content::Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK,
      false);
  wc_->OpenURL(params);
  return false;
}
