// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NON_VIEW_H_

#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view_model.h"

class OmniboxEditModel;

// A view representing the contents of the omnibox popup.  This headless view is
// used when Instant is doing the actual rendering of the suggestions.  The
// Omnibox still needs a model-like object, so that is provided here.  But the
// view portion itself is not displayed.
class OmniboxPopupNonView : public OmniboxResultViewModel,
                            public OmniboxPopupView {
 public:
  explicit OmniboxPopupNonView(OmniboxEditModel* edit_model);

  // Overridden from AutocompletePopupView:
  virtual bool IsOpen() const OVERRIDE;
  virtual void InvalidateLine(size_t line) OVERRIDE;
  virtual void UpdatePopupAppearance() OVERRIDE;
  virtual gfx::Rect GetTargetBounds() OVERRIDE;
  virtual void PaintUpdatesNow() OVERRIDE;
  virtual void OnDragCanceled() OVERRIDE;

  // Overridden from OmniboxResultViewModel:
  virtual bool IsSelectedIndex(size_t index) const OVERRIDE;
  virtual bool IsHoveredIndex(size_t index) const OVERRIDE;
  virtual gfx::Image GetIconIfExtensionMatch(size_t index) const OVERRIDE;

 protected:
  virtual ~OmniboxPopupNonView();

 private:
  OmniboxPopupModel model_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupNonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NON_VIEW_H_
