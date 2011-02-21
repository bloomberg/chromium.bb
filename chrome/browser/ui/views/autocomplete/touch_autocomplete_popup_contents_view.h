// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_TOUCH_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_TOUCH_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_result_view.h"

class AutocompleteEditView;
class AutocompleteEditModel;
class Profile;

namespace gfx {
class Canvas;
class CanvasSkia;
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
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void PaintMatch(gfx::Canvas* canvas,
                          const AutocompleteMatch& match,
                          int x);

  int GetFontHeight() const;

  DISALLOW_COPY_AND_ASSIGN(TouchAutocompleteResultView);
};

class TouchAutocompletePopupContentsView
    : public AutocompletePopupContentsView {
 public:
  TouchAutocompletePopupContentsView(const gfx::Font& font,
                                     AutocompleteEditView* edit_view,
                                     AutocompleteEditModel* edit_model,
                                     Profile* profile,
                                     const views::View* location_bar);
  virtual ~TouchAutocompletePopupContentsView();

  // AutocompletePopupContentsView:
  virtual void UpdatePopupAppearance();
  virtual void LayoutChildren();

 protected:
  // AutocompletePopupContentsView:
  virtual void PaintResultViews(gfx::CanvasSkia* canvas);
  virtual int CalculatePopupHeight();
  virtual AutocompleteResultView* CreateResultView(
      AutocompleteResultViewModel* model,
      int model_index,
      const gfx::Font& font,
      const gfx::Font& bold_font);

 private:
  std::vector<View*> GetVisibleChildren();

  DISALLOW_COPY_AND_ASSIGN(TouchAutocompletePopupContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_TOUCH_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
