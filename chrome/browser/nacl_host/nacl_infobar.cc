// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_infobar.h"

#include "base/bind.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/api/infobars/simple_alert_infobar_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ui/base/l10n/l10n_util.h"


using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

namespace {

void ShowInfobar(int render_process_id, int render_view_id,
                 int error_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(static_cast<PP_NaClError>(error_id) ==
        PP_NACL_MANIFEST_MISSING_ARCH);
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  WebContents* wc = WebContents::FromRenderViewHost(rvh);
  InfoBarService* ibs = InfoBarService::FromWebContents(wc);
  SimpleAlertInfoBarDelegate::Create(ibs, NULL,
        l10n_util::GetStringUTF16(IDS_NACL_APP_MISSING_ARCH_MESSAGE), true);
}

}  // namespace

void ShowNaClInfobar(int render_process_id, int render_view_id,
                     int error_id) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ShowInfobar, render_process_id, render_view_id,
                 error_id));
}
