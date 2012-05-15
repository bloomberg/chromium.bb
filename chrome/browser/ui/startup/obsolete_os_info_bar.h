// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_OBSOLETE_OS_INFO_BAR_H_
#define CHROME_BROWSER_UI_STARTUP_OBSOLETE_OS_INFO_BAR_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/link_infobar_delegate.h"
#include "googleurl/src/gurl.h"

class InfoBarTabHelper;

namespace browser {

// An infobar that is run with a string and a "Learn More" link.
class ObsoleteOSInfoBar : public LinkInfoBarDelegate {
 public:
  ObsoleteOSInfoBar(InfoBarTabHelper* infobar_helper,
                    const string16& message,
                    const GURL& url);
  virtual ~ObsoleteOSInfoBar();

  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

 private:
  const string16 message_;
  const GURL learn_more_url_;

  DISALLOW_COPY_AND_ASSIGN(ObsoleteOSInfoBar);
};

}  // namespace browser

#endif  // CHROME_BROWSER_UI_STARTUP_OBSOLETE_OS_INFO_BAR_H_
