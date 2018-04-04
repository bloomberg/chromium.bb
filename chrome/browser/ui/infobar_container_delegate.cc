// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/infobar_container_delegate.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/window/non_client_view.h"
#endif

// static
#if defined(OS_MACOSX)
const int InfoBarContainerDelegate::kSeparatorLineHeight = 1;
#else
const int InfoBarContainerDelegate::kSeparatorLineHeight =
    views::NonClientFrameView::kClientEdgeThickness;
#endif
