// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INFOBAR_CONTAINER_DELEGATE_H_
#define CHROME_BROWSER_UI_INFOBAR_CONTAINER_DELEGATE_H_

#include "components/infobars/core/infobar_container.h"

class InfoBarContainerDelegate : public infobars::InfoBarContainer::Delegate {
 public:
  static const int kSeparatorLineHeight;
};

#endif  // CHROME_BROWSER_UI_INFOBAR_CONTAINER_DELEGATE_H_
