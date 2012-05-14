// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_TOUCH_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_TOUCH_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_result_view.h"

class AutocompleteEditModel;
class OmniboxView;

namespace gfx {
class Canvas;
}

namespace views {
class View;
}

class TouchAutocompleteResultView : public AutocompleteResultView {
 public:
  TouchAutocompleteResultView(AutocompleteResultViewModel* model,
                              int model_index,
                              const gfx::Font& font,
                              const gfx::Font& bold_font);

 private:
  virtual ~TouchAutocompleteResultView();

  // AutocompleteResultView:
  virtual void PaintMatch(gfx::Canvas* canvas,
                          const AutocompleteMatch& match,
                          int x) OVERRIDE;
  virtual int GetTextHeight() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TouchAutocompleteResultView);
};

class TouchAutocompletePopupContentsView
    : public AutocompletePopupContentsView {
 public:
  TouchAutocompletePopupContentsView(const gfx::Font& font,
                                     OmniboxView* omnibox_view,
                                     AutocompleteEditModel* edit_model,
                                     views::View* location_bar);
  virtual ~TouchAutocompletePopupContentsView();

  // AutocompletePopupContentsView:
  virtual void UpdatePopupAppearance() OVERRIDE;

 protected:
  // AutocompletePopupContentsView:
  virtual void PaintResultViews(gfx::Canvas* canvas) OVERRIDE;
  virtual AutocompleteResultView* CreateResultView(
      AutocompleteResultViewModel* model,
      int model_index,
      const gfx::Font& font,
      const gfx::Font& bold_font) OVERRIDE;

 private:
  std::vector<View*> GetVisibleChildren();

  DISALLOW_COPY_AND_ASSIGN(TouchAutocompletePopupContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_TOUCH_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
