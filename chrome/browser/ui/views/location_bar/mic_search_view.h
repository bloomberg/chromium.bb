// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MIC_SEARCH_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MIC_SEARCH_VIEW_H_

#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "ui/views/controls/button/image_button.h"

class CommandUpdater;

class MicSearchView : public views::ImageButton,
                      public TouchableLocationBarView {
 public:
  explicit MicSearchView(views::ButtonListener* button_listener);
  virtual ~MicSearchView();

  // TouchableLocationBarView:
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MicSearchView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MIC_SEARCH_VIEW_H_
