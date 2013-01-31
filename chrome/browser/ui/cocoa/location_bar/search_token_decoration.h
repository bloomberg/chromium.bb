// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_SEARCH_TOKEN_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_SEARCH_TOKEN_DECORATION_H_

#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

#import "base/memory/scoped_nsobject.h"
#include "base/string16.h"

// This class is used to draw a label on the right side of the omnibox when
// the user does a search. For example, if the default search provider is
// google.com and the user does a search then the decoration would display
// "Google Search".
class SearchTokenDecoration : public LocationBarDecoration {
 public:
  SearchTokenDecoration();
  virtual ~SearchTokenDecoration();

  void SetSearchProviderName(const string16& search_provider_name);

  // Implement LocationBarDecoration:
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;
  virtual CGFloat GetWidthForSpace(CGFloat width, CGFloat text_width) OVERRIDE;

 private:
  string16 search_provider_name_;
  scoped_nsobject<NSAttributedString> search_provider_attributed_string_;

  DISALLOW_COPY_AND_ASSIGN(SearchTokenDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_SEARCH_TOKEN_DECORATION_H_
