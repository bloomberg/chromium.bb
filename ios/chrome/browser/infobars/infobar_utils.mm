// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_utils.h"

#include <memory>
#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ios/chrome/browser/infobars/confirm_infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar.h"

std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  std::unique_ptr<InfoBarIOS> infobar(new InfoBarIOS(std::move(delegate)));
  base::scoped_nsobject<ConfirmInfoBarController> controller(
      [[ConfirmInfoBarController alloc] initWithDelegate:infobar.get()]);
  infobar->SetController(controller);
  return std::move(infobar);
}
