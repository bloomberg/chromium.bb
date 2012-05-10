// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_prompt.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/link_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

class LearnMoreInfoBar : public LinkInfoBarDelegate {
 public:
  LearnMoreInfoBar(InfoBarTabHelper* infobar_helper,
                   const string16& message,
                   const GURL& url);
  virtual ~LearnMoreInfoBar();

  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

 private:
  string16 message_;
  GURL learn_more_url_;

  DISALLOW_COPY_AND_ASSIGN(LearnMoreInfoBar);
};

LearnMoreInfoBar::LearnMoreInfoBar(InfoBarTabHelper* infobar_helper,
                                   const string16& message,
                                   const GURL& url)
    : LinkInfoBarDelegate(infobar_helper),
      message_(message),
      learn_more_url_(url) {
}

LearnMoreInfoBar::~LearnMoreInfoBar() {
}

string16 LearnMoreInfoBar::GetMessageTextWithOffset(size_t* link_offset) const {
  string16 text = message_;
  text.push_back(' ');  // Add a space before the following link.
  *link_offset = text.size();
  return text;
}

string16 LearnMoreInfoBar::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool LearnMoreInfoBar::LinkClicked(WindowOpenDisposition disposition) {
  OpenURLParams params(
      learn_more_url_, Referrer(), disposition, content::PAGE_TRANSITION_LINK,
      false);
  owner()->web_contents()->OpenURL(params);
  return false;
}

}  // namespace

namespace browser {

void ShowObsoleteOSPrompt(Browser* browser) {
  // We've deprecated support for Ubuntu Hardy.  Rather than attempting to
  // determine whether you're using that, we instead key off the GTK version;
  // this will also deprecate other distributions (including variants of Ubuntu)
  // that are of a similar age.
  // Version key:
  //   Ubuntu Hardy: GTK 2.12
  //   RHEL 6:       GTK 2.18
  //   Ubuntu Lucid: GTK 2.20
  if (gtk_check_version(2, 18, 0)) {
    string16 message = l10n_util::GetStringUTF16(IDS_SYSTEM_OBSOLETE_MESSAGE);
    // Link to an article in the help center on minimum system requirements.
    const char* kLearnMoreURL =
        "http://www.google.com/support/chrome/bin/answer.py?answer=95411";
    TabContentsWrapper* tab = browser->GetSelectedTabContentsWrapper();
    if (!tab)
      return;
    tab->infobar_tab_helper()->AddInfoBar(
        new LearnMoreInfoBar(tab->infobar_tab_helper(),
                             message,
                             GURL(kLearnMoreURL)));
  }
}

}  // namespace browser
