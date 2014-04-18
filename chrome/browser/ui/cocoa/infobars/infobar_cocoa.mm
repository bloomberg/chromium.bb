// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_cocoa.h"

#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"

const int infobars::InfoBar::kSeparatorLineHeight = 1;
const int infobars::InfoBar::kDefaultArrowTargetHeight = 11;
const int infobars::InfoBar::kMaximumArrowTargetHeight = 24;
const int infobars::InfoBar::kDefaultArrowTargetHalfWidth =
    kDefaultArrowTargetHeight;
const int infobars::InfoBar::kMaximumArrowTargetHalfWidth = 14;
const int infobars::InfoBar::kDefaultBarTargetHeight = 36;

InfoBarCocoa::InfoBarCocoa(scoped_ptr<infobars::InfoBarDelegate> delegate)
    : infobars::InfoBar(delegate.Pass()),
      weak_ptr_factory_(this) {
}

InfoBarCocoa::~InfoBarCocoa() {
  if (controller())
    [controller() infobarWillClose];
}

infobars::InfoBarManager* InfoBarCocoa::OwnerCocoa() { return owner(); }

base::WeakPtr<InfoBarCocoa> InfoBarCocoa::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
