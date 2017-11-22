// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

namespace autofill {

class AutofillPopupController;

// Views implementation for the autofill and password suggestion.
// TODO(https://crbug.com/768881): Once this implementation is complete, this
// class should be renamed to AutofillPopupViewViews and old
// AutofillPopupViewViews should be removed. The main difference of
// AutofillPopupViewNativeViews from AutofillPopupViewViews is that child views
// are drawn using toolkit-views framework, in contrast to
// AutofillPopupViewViews, where individuals rows are drawn directly on canvas.
class AutofillPopupViewNativeViews : public AutofillPopupBaseView,
                                     public AutofillPopupView {
 public:
  AutofillPopupViewNativeViews(AutofillPopupController* controller,
                               views::Widget* parent_widget);
  ~AutofillPopupViewNativeViews() override;

  void Show() override;
  void Hide() override;

 private:
  void OnSelectedRowChanged(base::Optional<int> previous_row_selection,
                            base::Optional<int> current_row_selection) override;
  void OnSuggestionsChanged() override;

  // Creates child views based on the suggestions given by |controller_|.
  void CreateChildViews();

  // Controller for this view.
  AutofillPopupController* controller_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewNativeViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
