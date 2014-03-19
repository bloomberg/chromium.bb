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
  TouchOmniboxResultView(OmniboxPopupContentsView* model,
                         int model_index,
                         LocationBarView* location_bar_view,
                         const gfx::FontList& font_list);

 private:
  virtual ~TouchOmniboxResultView();

  // OmniboxResultView:
  virtual void PaintMatch(const AutocompleteMatch& match,
                          gfx::RenderText* contents,
                          gfx::RenderText* description,
                          gfx::Canvas* canvas,
                          int x) const OVERRIDE;
  virtual int GetTextHeight() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TouchOmniboxResultView);
};

class TouchOmniboxPopupContentsView
    : public OmniboxPopupContentsView {
 public:
  TouchOmniboxPopupContentsView(const gfx::FontList& font_list,
                                OmniboxView* omnibox_view,
                                OmniboxEditModel* edit_model,
                                LocationBarView* location_bar_view);
  virtual ~TouchOmniboxPopupContentsView();

  // OmniboxPopupContentsView:
  virtual void UpdatePopupAppearance() OVERRIDE;

 protected:
  // OmniboxPopupContentsView:
  virtual void PaintResultViews(gfx::Canvas* canvas) OVERRIDE;
  virtual OmniboxResultView* CreateResultView(
      int model_index,
      const gfx::FontList& font_list) OVERRIDE;

 private:
  std::vector<View*> GetVisibleChildren();

  DISALLOW_COPY_AND_ASSIGN(TouchOmniboxPopupContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_TOUCH_OMNIBOX_POPUP_CONTENTS_VIEW_H_
