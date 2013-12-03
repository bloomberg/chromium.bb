// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_cocoa.h"

const int InfoBar::kSeparatorLineHeight = 1;
const int InfoBar::kDefaultArrowTargetHeight = 11;
const int InfoBar::kMaximumArrowTargetHeight = 24;
const int InfoBar::kDefaultArrowTargetHalfWidth = kDefaultArrowTargetHeight;
const int InfoBar::kMaximumArrowTargetHalfWidth = 14;
const int InfoBar::kDefaultBarTargetHeight = 36;

InfoBarCocoa::InfoBarCocoa(InfoBarService* owner, InfoBarDelegate* delegate)
    : InfoBar(owner, delegate),
      weak_ptr_factory_(this) {
}

InfoBarCocoa::~InfoBarCocoa() {
}

void InfoBarCocoa::RemoveSelfCocoa() {
  RemoveSelf();
}

InfoBarService* InfoBarCocoa::OwnerCocoa() {
  return owner();
}

base::WeakPtr<InfoBarCocoa> InfoBarCocoa::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
