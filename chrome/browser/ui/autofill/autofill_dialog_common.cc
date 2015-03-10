// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_common.h"

namespace autofill {
namespace common {

AutofillMetrics::DialogUiEvent DialogSectionToUiItemAddedEvent(
    DialogSection section) {
  switch (section) {
    case SECTION_BILLING:
      return AutofillMetrics::DIALOG_UI_BILLING_ITEM_ADDED;

    case SECTION_CC_BILLING:
      return AutofillMetrics::DIALOG_UI_CC_BILLING_ITEM_ADDED;

    case SECTION_SHIPPING:
      return AutofillMetrics::DIALOG_UI_SHIPPING_ITEM_ADDED;

    case SECTION_CC:
      return AutofillMetrics::DIALOG_UI_CC_ITEM_ADDED;
  }

  NOTREACHED();
  return AutofillMetrics::NUM_DIALOG_UI_EVENTS;
}

AutofillMetrics::DialogUiEvent DialogSectionToUiSelectionChangedEvent(
    DialogSection section) {
  switch (section) {
    case SECTION_BILLING:
      return AutofillMetrics::DIALOG_UI_BILLING_SELECTED_SUGGESTION_CHANGED;

    case SECTION_CC_BILLING:
      return AutofillMetrics::DIALOG_UI_CC_BILLING_SELECTED_SUGGESTION_CHANGED;

    case SECTION_SHIPPING:
      return AutofillMetrics::DIALOG_UI_SHIPPING_SELECTED_SUGGESTION_CHANGED;

    case SECTION_CC:
      return AutofillMetrics::DIALOG_UI_CC_SELECTED_SUGGESTION_CHANGED;
  }

  NOTREACHED();
  return AutofillMetrics::NUM_DIALOG_UI_EVENTS;
}

}  // namespace common
}  // namespace autofill
