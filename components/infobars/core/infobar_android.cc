// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar.h"

// Static constants defined in infobar.h.  We don't really use them for anything
// but they are required.  The values are copied from the GTK implementation.
const int infobars::InfoBar::kSeparatorLineHeight = 1;
const int infobars::InfoBar::kDefaultArrowTargetHeight = 9;
const int infobars::InfoBar::kMaximumArrowTargetHeight = 24;
const int infobars::InfoBar::kDefaultArrowTargetHalfWidth =
    kDefaultArrowTargetHeight;
const int infobars::InfoBar::kMaximumArrowTargetHalfWidth = 14;
const int infobars::InfoBar::kDefaultBarTargetHeight = 36;
