// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/infobars/infobar.h"

@class InfoBarController;

// The cocoa specific implementation of InfoBar. The real info bar logic is
// actually in InfoBarController.
class InfoBarCocoa : public InfoBar {
 public:
  InfoBarCocoa(InfoBarService* owner, InfoBarDelegate* delegate);

  virtual ~InfoBarCocoa();

  InfoBarController* controller() const { return controller_; }

  void set_controller(InfoBarController* controller) {
    controller_.reset([controller retain]);
  }

  // These functions allow access to protected InfoBar functions.
  void RemoveSelfCocoa();
  InfoBarService* OwnerCocoa();

 private:
  // The Objective-C class that contains most of the info bar logic.
  base::scoped_nsobject<InfoBarController> controller_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_COCOA_H_
