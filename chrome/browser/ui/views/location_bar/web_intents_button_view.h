// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEB_INTENTS_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEB_INTENTS_BUTTON_VIEW_H_

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/views/location_bar/location_bar_decoration_view.h"

class TabContents;

// Display the use-another-service button for web intents service pages
// displayed in a tab.
class WebIntentsButtonView : public LocationBarDecorationView {
 public:
  // |parent| and |background_images| passed to superclass. They are a weak ptr
  // to owning class and the background images for the button background.
  WebIntentsButtonView(LocationBarView* parent, const int background_images[]);
  virtual ~WebIntentsButtonView() {}

  virtual void Update(TabContents* tab_contents) OVERRIDE;

 protected:
  virtual void OnClick(LocationBarView* parent) OVERRIDE;
  virtual int GetTextAnimationSize(double state, int text_size) OVERRIDE;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebIntentsButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEB_INTENTS_BUTTON_VIEW_H_
