// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_TOUCH_OMNIBOX_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_TOUCH_OMNIBOX_POPUP_CONTENTS_VIEW_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

class OmniboxEditModel;
class OmniboxView;

namespace gfx {
class Canvas;
}

namespace views {
class View;
}

class TouchOmniboxResultView : public OmniboxResultView {
 public:
  TouchOmniboxResultView(OmniboxResultViewModel* model,
                         int model_index,
                         views::View* location_bar,
                         const gfx::Font& font);

 private:
  virtual ~TouchOmniboxResultView();

  // OmniboxResultView:
  virtual void PaintMatch(gfx::Canvas* canvas,
                          const AutocompleteMatch& match,
                          int x) OVERRIDE;
  virtual int GetTextHeight() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TouchOmniboxResultView);
};

class TouchOmniboxPopupContentsView
    : public OmniboxPopupContentsView {
 public:
  TouchOmniboxPopupContentsView(const gfx::Font& font,
                                OmniboxView* omnibox_view,
                                OmniboxEditModel* edit_model,
                                views::View* location_bar);
  virtual ~TouchOmniboxPopupContentsView();

  // OmniboxPopupContentsView:
  virtual void UpdatePopupAppearance() OVERRIDE;

 protected:
  // OmniboxPopupContentsView:
  virtual void PaintResultViews(gfx::Canvas* canvas) OVERRIDE;
  virtual OmniboxResultView* CreateResultView(OmniboxResultViewModel* model,
                                              int model_index,
                                              const gfx::Font& font) OVERRIDE;

 private:
  std::vector<View*> GetVisibleChildren();

  DISALLOW_COPY_AND_ASSIGN(TouchOmniboxPopupContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_TOUCH_OMNIBOX_POPUP_CONTENTS_VIEW_H_
