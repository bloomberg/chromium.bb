// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_OBSOLETE_OS_INFO_BAR_H_
#define CHROME_BROWSER_UI_STARTUP_OBSOLETE_OS_INFO_BAR_H_

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "googleurl/src/gurl.h"

class InfoBarService;

namespace chrome {

// An infobar that is run with a string and a "Learn More" link.
class ObsoleteOSInfoBar : public ConfirmInfoBarDelegate {
 public:
  ObsoleteOSInfoBar(InfoBarService* infobar_service,
                    const string16& message,
                    const GURL& url);
  virtual ~ObsoleteOSInfoBar();

 private:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  const string16 message_;
  const GURL learn_more_url_;

  DISALLOW_COPY_AND_ASSIGN(ObsoleteOSInfoBar);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_STARTUP_OBSOLETE_OS_INFO_BAR_H_
