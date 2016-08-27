// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"


// static
void NaClInfoBarDelegate::Create(int render_process_id, int render_view_id) {
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!rvh)
    return;
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return;
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (infobar_service) {
    infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
        std::unique_ptr<ConfirmInfoBarDelegate>(new NaClInfoBarDelegate())));
  }
}

NaClInfoBarDelegate::NaClInfoBarDelegate() : ConfirmInfoBarDelegate() {
}

NaClInfoBarDelegate::~NaClInfoBarDelegate() {
}

infobars::InfoBarDelegate::InfoBarIdentifier
NaClInfoBarDelegate::GetIdentifier() const {
  return NACL_INFOBAR_DELEGATE;
}

base::string16 NaClInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_NACL_APP_MISSING_ARCH_MESSAGE);
}

int NaClInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 NaClInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL NaClInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome/?p=ib_nacl");
}
