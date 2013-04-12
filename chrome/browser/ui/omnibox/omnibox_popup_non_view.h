// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_NON_VIEW_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_NON_VIEW_H_

#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"

class OmniboxEditModel;

// A view representing the contents of the omnibox popup.  This headless view is
// used when Instant is doing the actual rendering of the suggestions.  The
// Omnibox still needs a model-like object, so that is provided here.  But the
// view portion itself is not displayed.
class OmniboxPopupNonView : public OmniboxPopupView {
 public:
  explicit OmniboxPopupNonView(OmniboxEditModel* edit_model);

  // Overridden from OmniboxPopupView:
  virtual bool IsOpen() const OVERRIDE;
  virtual void InvalidateLine(size_t line) OVERRIDE;
  virtual void UpdatePopupAppearance() OVERRIDE;
  virtual gfx::Rect GetTargetBounds() OVERRIDE;
  virtual void PaintUpdatesNow() OVERRIDE;
  virtual void OnDragCanceled() OVERRIDE;

 protected:
  virtual ~OmniboxPopupNonView();

 private:
  OmniboxPopupModel model_;

  // |is_open_| reflects whether the OmniboxEditModel has a non-empty result
  // set. However, we can't get rid of this variable and just use the model's
  // state directly, as there's at least one situation when the model's result
  // set has been emptied, but the popup should still be considered to be open
  // until UpdatePopupAppearance() is called (cf: |was_open| in
  // OmniboxEditModel::OnResultChanged()).
  bool is_open_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupNonView);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_NON_VIEW_H_
