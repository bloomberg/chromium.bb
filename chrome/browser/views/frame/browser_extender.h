// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_EXTENDER_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_EXTENDER_H_

#include "base/basictypes.h"
#include "gfx/rect.h"

class BrowserView;

// Note: This class is deprecated (but still in use)
// and will be removed in the near future.
//
// BrowserExtender adds chromeos specific features to BrowserView.
// The factory method |Create(BrowserView*)| creates different types
// of extender depending on the type of BrowserView and target platform.
// Please see browser/chromeos/browser_extenders.cc for ChromeOS extenders, and
// standard_extender.cc for Chrome browser.
class BrowserExtender {
 public:
  BrowserExtender()
      : can_close_(true) {
  }

  virtual ~BrowserExtender() {}

  // Tells if the browser can be closed.
  bool can_close() const {
    return can_close_;
  }

  // Specifies if the browser can be closed or not. This typically set
  // to false when the browser is being dragged.
  void set_can_close(bool b) {
    can_close_ = b;
  }

 private:
  // True if the browser can be closed. See set_can_close method for setails.
  bool can_close_;

  DISALLOW_COPY_AND_ASSIGN(BrowserExtender);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_EXTENDER_H_
