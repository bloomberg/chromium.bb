// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_views.h"

#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"

namespace autofill {

AutofillPopupViewViews::AutofillPopupViewViews(
    AutofillPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {}

AutofillPopupViewViews::~AutofillPopupViewViews() {}

void AutofillPopupViewViews::Show() {
  DoShow();
}

void AutofillPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = NULL;

  DoHide();
}

void AutofillPopupViewViews::UpdateBoundsAndRedrawPopup() {
  DoUpdateBoundsAndRedrawPopup();
}

void AutofillPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  canvas->DrawColor(kPopupBackground);
  OnPaintBorder(canvas);

  for (size_t i = 0; i < controller_->GetLineCount(); ++i) {
    gfx::Rect line_rect = controller_->layout_model().GetRowBounds(i);

    if (controller_->GetSuggestionAt(i).frontend_id ==
        POPUP_ITEM_ID_SEPARATOR) {
      canvas->FillRect(line_rect, kLabelTextColor);
    } else {
      DrawAutofillEntry(canvas, i, line_rect);
    }
  }
}

void AutofillPopupViewViews::InvalidateRow(size_t row) {
  SchedulePaintInRect(controller_->layout_model().GetRowBounds(row));
}

/**
* Autofill entries in ltr.
*
* ............................................................................
* . ICON | HTTP WARNING MESSAGE VALUE                                | LABEL .
* ............................................................................
* . OTHER AUTOFILL ENTRY VALUE |                                LABEL | ICON .
* ............................................................................
*
* Autofill entries in rtl.
*
* ............................................................................
* . LABEL |                                HTTP WARNING MESSAGE VALUE | ICON .
* ............................................................................
* . ICON | LABEL                                | OTHER AUTOFILL ENTRY VALUE .
* ............................................................................
*
* Anyone who wants to modify the code below, remember to make sure that HTTP
* warning entry displays right. To trigger the warning message entry, enable
* #mark-non-secure-as flag as "display form warning", go to goo.gl/CEIjc6 with
* stored autofill info and check for credit card or password forms.
*/
void AutofillPopupViewViews::DrawAutofillEntry(gfx::Canvas* canvas,
                                               int index,
                                               const gfx::Rect& entry_rect) {
  canvas->FillRect(entry_rect, controller_->GetBackgroundColorForRow(index));

  const bool is_http_warning =
      (controller_->GetSuggestionAt(index).frontend_id ==
       POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE);
  const bool is_rtl = controller_->IsRTL();
  const int text_align =
      is_rtl ? gfx::Canvas::TEXT_ALIGN_RIGHT : gfx::Canvas::TEXT_ALIGN_LEFT;
  gfx::Rect value_rect = entry_rect;
  value_rect.Inset(AutofillPopupLayoutModel::kEndPadding, 0);

  // If the icon is on the right of the rect, no matter in RTL or LTR mode.
  bool icon_on_the_right = is_http_warning == is_rtl;
  int x_align_left = icon_on_the_right ? value_rect.right() : value_rect.x();

  // Draw the Autofill icon, if one exists
  int row_height = controller_->layout_model().GetRowBounds(index).height();
  if (!controller_->GetSuggestionAt(index).icon.empty()) {
    const gfx::ImageSkia image =
        controller_->layout_model().GetIconImage(index);
    int icon_y = entry_rect.y() + (row_height - image.height()) / 2;

    int icon_x_align_left =
        icon_on_the_right ? x_align_left - image.width() : x_align_left;

    canvas->DrawImageInt(image, icon_x_align_left, icon_y);

    // An icon was drawn; adjust the |x_align_left| value for the next element.
    if (is_http_warning) {
      x_align_left =
          icon_x_align_left +
          (is_rtl ? -AutofillPopupLayoutModel::kHttpWarningIconPadding
                  : image.width() +
                        AutofillPopupLayoutModel::kHttpWarningIconPadding);
    } else {
      x_align_left =
          icon_x_align_left +
          (is_rtl ? image.width() + AutofillPopupLayoutModel::kIconPadding
                  : -AutofillPopupLayoutModel::kIconPadding);
    }
  }

  // Draw the value text
  const int value_width = gfx::GetStringWidth(
      controller_->GetElidedValueAt(index),
      controller_->layout_model().GetValueFontListForRow(index));
  int value_x_align_left = x_align_left;

  if (is_http_warning) {
    value_x_align_left += is_rtl ? -value_width : 0;
  } else {
    value_x_align_left =
        is_rtl ? value_rect.right() - value_width : value_rect.x();
  }

  canvas->DrawStringRectWithFlags(
      controller_->GetElidedValueAt(index),
      controller_->layout_model().GetValueFontListForRow(index),
      controller_->layout_model().GetValueFontColorForRow(index),
      gfx::Rect(value_x_align_left, value_rect.y(), value_width,
                value_rect.height()),
      text_align);

  // Draw the label text, if one exists.
  if (!controller_->GetSuggestionAt(index).label.empty()) {
    const int label_width = gfx::GetStringWidth(
        controller_->GetElidedLabelAt(index),
        controller_->layout_model().GetLabelFontListForRow(index));
    int label_x_align_left = x_align_left;

    if (is_http_warning) {
      label_x_align_left =
          is_rtl ? value_rect.x() : value_rect.right() - label_width;
    } else {
      label_x_align_left += is_rtl ? 0 : -label_width;
    }

    canvas->DrawStringRectWithFlags(
        controller_->GetElidedLabelAt(index),
        controller_->layout_model().GetLabelFontListForRow(index),
        kLabelTextColor, gfx::Rect(label_x_align_left, entry_rect.y(),
                                   label_width, entry_rect.height()),
        text_align);
  }
}

AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up.
  if (!observing_widget)
    return NULL;

  return new AutofillPopupViewViews(controller, observing_widget);
}

}  // namespace autofill
