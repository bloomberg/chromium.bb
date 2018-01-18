// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_PUBLIC_CWV_PAGE_TRANSITION_INTERNAL_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_PAGE_TRANSITION_INTERNAL_H_

#include "ios/web_view/public/cwv_navigation_type.h"
#include "ui/base/page_transition_types.h"

NS_ASSUME_NONNULL_BEGIN

// Converts ui::PageTransition to CWVNavigationType.
CWVNavigationType CWVNavigationTypeFromPageTransition(
    ui::PageTransition ui_page_transition);

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_PAGE_TRANSITION_INTERNAL_H_
