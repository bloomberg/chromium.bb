// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_H_
#pragma once

#include "base/logging.h"  // for DCHECK

@class InfoBarController;

// A C++ wrapper around an Objective-C InfoBarController.  This class
// exists solely to be the return value for InfoBarDelegate::CreateInfoBar(),
// as defined in chrome/browser/tab_contents/confirm_infobar_delegate.h.  This
// class would be analogous to the various bridge classes we already
// have, but since there is no pre-defined InfoBar interface, it is
// easier to simply throw away this object and deal with the
// controller directly rather than pass messages through a bridge.
//
// Callers should delete the returned InfoBar immediately after
// calling CreateInfoBar(), as the returned InfoBar* object is not
// pointed to by anyone.  Expected usage:
//
// scoped_ptr<InfoBar> infobar(delegate->CreateInfoBar());
// InfoBarController* controller = infobar->controller();
// // Do something with the controller, and save a pointer so it can be
// // deleted later.  |infobar| will be deleted automatically.

class InfoBar {
 public:
  InfoBar(InfoBarController* controller) {
    DCHECK(controller);
    controller_ = controller;
  }

  InfoBarController* controller() {
    return controller_;
  }

 private:
  // Pointer to the infobar controller.  Is never null.
  InfoBarController* controller_;  // weak

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_H_
