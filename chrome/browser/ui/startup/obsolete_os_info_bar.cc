// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_info_bar.h"

#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;

namespace browser {

ObsoleteOSInfoBar::ObsoleteOSInfoBar(InfoBarTabHelper* infobar_helper,
                                     const string16& message,
                                     const GURL& url)
    : LinkInfoBarDelegate(infobar_helper),
      message_(message),
      learn_more_url_(url) {
}

ObsoleteOSInfoBar::~ObsoleteOSInfoBar() {
}

string16 ObsoleteOSInfoBar::GetMessageTextWithOffset(
    size_t* link_offset) const {
  string16 text = message_;
  text.push_back(' ');  // Add a space before the following link.
  *link_offset = text.size();
  return text;
}

string16 ObsoleteOSInfoBar::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ObsoleteOSInfoBar::LinkClicked(WindowOpenDisposition disposition) {
  OpenURLParams params(
      learn_more_url_, Referrer(), disposition, content::PAGE_TRANSITION_LINK,
      false);
  owner()->web_contents()->OpenURL(params);
  return false;
}

}  // namespace browser
