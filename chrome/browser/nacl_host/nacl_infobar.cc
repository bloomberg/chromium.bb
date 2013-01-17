// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_infobar.h"

#include "base/bind.h"
#include "base/string16.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

namespace {
// The URL for the "learn more" article.
const char kNaClLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=ib_nacl";

// A simple LinkInfoBarDelegate doesn't support making the link right-aligned
// so use a ConfirmInfoBarDelegate without any buttons instead.
class NaClInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  static void Create(InfoBarService* ibs, WebContents* wc);
 private:
  NaClInfoBarDelegate(WebContents* wc, InfoBarService* ibs) :
      ConfirmInfoBarDelegate(ibs), wc_(wc) {}
  WebContents* wc_;
};

string16 NaClInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_NACL_APP_MISSING_ARCH_MESSAGE);
}

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

int NaClInfoBarDelegate::GetButtons() const { return BUTTON_NONE; }

// static
void NaClInfoBarDelegate::Create(InfoBarService* ibs, WebContents *wc) {
  ibs->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new NaClInfoBarDelegate(wc, ibs)));
}

void ShowInfobar(int render_process_id, int render_view_id,
                 int error_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(static_cast<PP_NaClError>(error_id) ==
        PP_NACL_MANIFEST_MISSING_ARCH);
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (!rvh)
    return;

  WebContents* wc = WebContents::FromRenderViewHost(rvh);
  if (!wc)
    return;

  InfoBarService* infobar_service = InfoBarService::FromWebContents(wc);
  if (infobar_service)
    NaClInfoBarDelegate::Create(infobar_service, wc);
}

}  // namespace

void ShowNaClInfobar(int render_process_id, int render_view_id,
                     int error_id) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ShowInfobar, render_process_id, render_view_id,
                 error_id));
}
