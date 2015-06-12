// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_SAVE_PASSWORD_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_SAVE_PASSWORD_INFOBAR_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"

namespace infobars {
class InfoBar;
}
class SavePasswordInfoBarDelegate;

scoped_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate);

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_SAVE_PASSWORD_INFOBAR_CONTROLLER_H_
