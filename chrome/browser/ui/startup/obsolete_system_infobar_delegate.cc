// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"

#include "base/cpu.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mac/obsolete_system.h"
#endif

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

// static
void ObsoleteSystemInfoBarDelegate::Create(InfoBarService* infobar_service) {
#if defined(OS_MACOSX)
  if (!ObsoleteSystemMac::Is32BitObsoleteNowOrSoon() ||
      !ObsoleteSystemMac::Has32BitOnlyCPU()) {
    return;
  }
#elif defined(TOOLKIT_GTK)
  // We've deprecated support for Ubuntu Lucid.  Rather than attempting to
  // determine whether you're using that, we instead key off the GTK version;
  // this will also deprecate other distributions (including variants of Ubuntu)
  // that are of a similar age.
  // Version key:
  //   RHEL 6:             GTK 2.18
  //   Debian 6 (Squeeze): GTK 2.20
  //   Ubuntu Lucid:       GTK 2.20
  //   openSUSE 12.2       GTK 2.24
  //   Ubuntu Precise:     GTK 2.24
  if (!gtk_check_version(2, 24, 0))
    return;
#elif defined(OS_WIN)
  // On Windows we no longer support non-SSE2 machines since Chrome 35.
  if (base::CPU().has_sse2())
    return;
#else
  // No other platforms currently show this infobar.
  return;
#endif

  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new ObsoleteSystemInfoBarDelegate())));
}

ObsoleteSystemInfoBarDelegate::ObsoleteSystemInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

ObsoleteSystemInfoBarDelegate::~ObsoleteSystemInfoBarDelegate() {
}

base::string16 ObsoleteSystemInfoBarDelegate::GetMessageText() const {
#if defined(OS_MACOSX)
  return ObsoleteSystemMac::LocalizedObsoleteSystemString();
#elif defined(OS_WIN)
  return l10n_util::GetStringUTF16(IDS_WIN_SSE_OBSOLETE_NOW);
#else
  return l10n_util::GetStringUTF16(IDS_SYSTEM_OBSOLETE_MESSAGE);
#endif
}

int ObsoleteSystemInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 ObsoleteSystemInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ObsoleteSystemInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
#if defined(OS_MACOSX)
          GURL(chrome::kMac32BitDeprecationURL),
#else
          GURL("http://www.google.com/support/chrome/bin/"
               "answer.py?answer=95411"),
#endif
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          content::PAGE_TRANSITION_LINK, false));
  return false;
}
