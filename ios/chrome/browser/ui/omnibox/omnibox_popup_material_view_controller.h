// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MATERIAL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MATERIAL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "components/omnibox/browser/autocomplete_result.h"

namespace image_fetcher {
class IOSImageDataFetcherWrapper;
}

class OmniboxPopupMaterialViewControllerDelegate {
 public:
  virtual bool IsStarredMatch(const AutocompleteMatch& match) const = 0;
  virtual void OnMatchSelected(const AutocompleteMatch& match, size_t row) = 0;
  virtual void OnMatchSelectedForAppending(const AutocompleteMatch& match) = 0;
  virtual void OnMatchSelectedForDeletion(const AutocompleteMatch& match) = 0;
  virtual void OnScroll() = 0;
};

// View controller used to display a list of omnibox autocomplete matches in the
// omnibox popup.
@interface OmniboxPopupMaterialViewController : UITableViewController

@property(nonatomic, assign) BOOL incognito;

// Designated initializer.  Creates a table view with UITableViewStylePlain.
// Takes ownership of |imageFetcher|.
- (instancetype)
initWithFetcher:
    (std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper>)imageFetcher
       delegate:(OmniboxPopupMaterialViewControllerDelegate*)delegate;

// Updates the current data and forces a redraw. If animation is YES, adds
// CALayer animations to fade the OmniboxPopupMaterialRows in.
- (void)updateMatches:(const AutocompleteResult&)result
        withAnimation:(BOOL)animation;

// Set text alignment for popup cells.
- (void)setTextAlignment:(NSTextAlignment)alignment;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MATERIAL_VIEW_CONTROLLER_H_
