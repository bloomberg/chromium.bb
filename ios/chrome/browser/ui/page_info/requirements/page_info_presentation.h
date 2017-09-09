// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_REQUIREMENTS_PAGE_INFO_PRESENTATION_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_REQUIREMENTS_PAGE_INFO_PRESENTATION_H_

@class UIView;

// PageInfoPresentation contains methods related to the presentation of the Page
// Info UI.
@protocol PageInfoPresentation

// The view in which the Page Info UI should be presented.
- (UIView*)viewForPageInfoPresentation;

// Called before the Page Info UI is presented.
- (void)prepareForPageInfoPresentation;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_REQUIREMENTS_PAGE_INFO_PRESENTATION_H_
