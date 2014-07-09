// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

// Provides dummy definitions of static variables and functions that are
// declared in the component but defined in the embedder in order to allow
// components_unittests to link.
// TODO(blundell): The component shouldn't be declaring statics that it's not
// defining. crbug.com/386171

// On Android, these variables are defined in ../core/infobar_android.cc.
#if !defined(OS_ANDROID)
const int infobars::InfoBar::kSeparatorLineHeight = 1;
const int infobars::InfoBar::kDefaultArrowTargetHeight = 9;
const int infobars::InfoBar::kMaximumArrowTargetHeight = 24;
const int infobars::InfoBar::kDefaultArrowTargetHalfWidth =
    kDefaultArrowTargetHeight;
const int infobars::InfoBar::kMaximumArrowTargetHalfWidth = 14;
const int infobars::InfoBar::kDefaultBarTargetHeight = 36;
#endif

scoped_ptr<infobars::InfoBar> ConfirmInfoBarDelegate::CreateInfoBar(
    scoped_ptr<ConfirmInfoBarDelegate> delegate) {
  NOTREACHED();
  return scoped_ptr<infobars::InfoBar>();
}
