// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_

#import <UIKit/UIKit.h>

namespace content_suggestions {

// Returns the maximum number of tiles fitting in |availableWidth|, limited to
// 4.
NSUInteger numberOfTilesForWidth(CGFloat availableWidth);
// Returns the spacing between tiles, based on the device.
CGFloat spacingBetweenTiles();

// iPhone landscape uses a slightly different layout for the doodle and search
// field frame. Returns the proper frame from |frames| based on orientation,
// centered in the view of |width|.
CGRect getOrientationFrame(const CGRect frames[], CGFloat width);
// Returns x-offset in order to have the tiles centered in a view with a
// |width|.
CGFloat centeredTilesMarginForWidth(CGFloat width);
// Returns the proper frame for the doodle inside a view with a |width|.
// |logoIsShowing| refers to the Google logo or the doodle.
CGRect doodleFrame(CGFloat width, BOOL logoIsShowing);
// Returns the proper frame for the search field inside a view with a |width|.
// |logoIsShowing| refers to the Google logo or the doodle.
CGRect searchFieldFrame(CGFloat width, BOOL logoIsShowing);
// Returns the expected height of the NewTabPageHeaderView inside a view with a
// |width|. |logoIsShowing| refers to the Google logo or the doodle.
// |promoCanShow| represents whether a what's new promo can be displayed.
CGFloat heightForLogoHeader(CGFloat width,
                            BOOL logoIsShowing,
                            BOOL promoCanShow);
// Configure the |searchHintLabel| for the fake omnibox, adding it to the
// |searchTapTarget| and constrain it.
void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIButton* searchTapTarget,
                              CGFloat searchFieldWidth);
// Configure the |voiceSearchButton|, adding it to the |searchTapTarget| and
// constraining it.
void configureVoiceSearchButton(UIButton* voiceSearchButton,
                                UIButton* searchTapTarget);

}  // namespace content_suggestions

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
