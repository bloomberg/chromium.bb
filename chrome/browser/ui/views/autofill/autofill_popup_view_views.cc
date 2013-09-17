// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_views.h"

#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "grit/ui_resources.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/border.h"
#include "ui/views/event_utils.h"
#include "ui/views/widget/widget.h"

using WebKit::WebAutofillClient;

namespace {

const SkColor kBorderColor = SkColorSetARGB(0xFF, 0xC7, 0xCA, 0xCE);
const SkColor kHoveredBackgroundColor = SkColorSetARGB(0xFF, 0xCD, 0xCD, 0xCD);
const SkColor kItemTextColor = SkColorSetARGB(0xFF, 0x7F, 0x7F, 0x7F);
const SkColor kPopupBackground = SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);
const SkColor kValueTextColor = SkColorSetARGB(0xFF, 0x00, 0x00, 0x00);
const SkColor kWarningTextColor = SkColorSetARGB(0xFF, 0x7F, 0x7F, 0x7F);

}  // namespace

namespace autofill {

AutofillPopupViewViews::AutofillPopupViewViews(
    AutofillPopupController* controller, views::Widget* observing_widget)
    : controller_(controller),
      observing_widget_(observing_widget) {}

AutofillPopupViewViews::~AutofillPopupViewViews() {
  if (controller_) {
    controller_->ViewDestroyed();

    HideInternal();
  }
}

void AutofillPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = NULL;

  HideInternal();

  if (GetWidget()) {
    // Don't call CloseNow() because some of the functions higher up the stack
    // assume the the widget is still valid after this point.
    // http://crbug.com/229224
    // NOTE: This deletes |this|.
    GetWidget()->Close();
  } else {
    delete this;
  }
}

void AutofillPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  canvas->DrawColor(kPopupBackground);
  OnPaintBorder(canvas);

  for (size_t i = 0; i < controller_->names().size(); ++i) {
    gfx::Rect line_rect = controller_->GetRowBounds(i);

    if (controller_->identifiers()[i] ==
            WebAutofillClient::MenuItemIDSeparator) {
      canvas->DrawRect(line_rect, kItemTextColor);
    } else {
      DrawAutofillEntry(canvas, i, line_rect);
    }
  }
}

void AutofillPopupViewViews::OnMouseCaptureLost() {
  if (controller_)
    controller_->MouseExitedPopup();
}

bool AutofillPopupViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  if (!controller_)
    return false;

  if (HitTestPoint(event.location())) {
    controller_->MouseHovered(event.x(), event.y());

    // We must return true in order to get future OnMouseDragged and
    // OnMouseReleased events.
    return true;
  }

  // If we move off of the popup, we lose the selection.
  controller_->MouseExitedPopup();
  return false;
}

void AutofillPopupViewViews::OnMouseExited(const ui::MouseEvent& event) {
  if (controller_)
    controller_->MouseExitedPopup();
}

void AutofillPopupViewViews::OnMouseMoved(const ui::MouseEvent& event) {
  if (!controller_)
    return;

  if (HitTestPoint(event.location()))
    controller_->MouseHovered(event.x(), event.y());
  else
    controller_->MouseExitedPopup();
}

bool AutofillPopupViewViews::OnMousePressed(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location()))
    return true;

  if (controller_->hide_on_outside_click()) {
    GetWidget()->ReleaseCapture();

    gfx::Point screen_loc = event.location();
    views::View::ConvertPointToScreen(this, &screen_loc);

    ui::MouseEvent mouse_event = event;
    mouse_event.set_location(screen_loc);

    if (controller_->ShouldRepostEvent(mouse_event)) {
      gfx::NativeView native_view = GetWidget()->GetNativeView();
      gfx::Screen* screen = gfx::Screen::GetScreenFor(native_view);
      gfx::NativeWindow window = screen->GetWindowAtScreenPoint(screen_loc);
      views::RepostLocatedEvent(window, mouse_event);
    }

    controller_->Hide();
    // |this| is now deleted.
  }

  return false;
}

void AutofillPopupViewViews::OnMouseReleased(const ui::MouseEvent& event) {
  if (!controller_)
    return;

  // Because this view can can be shown in response to a mouse press, it can
  // receive an OnMouseReleased event just after showing. This breaks the mouse
  // capture, so restart capturing here.
  if (controller_->hide_on_outside_click() && GetWidget())
    GetWidget()->SetCapture(this);

  // We only care about the left click.
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    controller_->MouseClicked(event.x(), event.y());
}

void AutofillPopupViewViews::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget, observing_widget_);
  controller_->Hide();
}

void AutofillPopupViewViews::Show() {
  if (!GetWidget()) {
    observing_widget_->AddObserver(this);

    // The widget is destroyed by the corresponding NativeWidget, so we use
    // a weak pointer to hold the reference and don't have to worry about
    // deletion.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.delegate = this;
    params.parent = controller_->container_view();
    widget->Init(params);
    widget->SetContentsView(this);
  }

  set_border(views::Border::CreateSolidBorder(kBorderThickness, kBorderColor));

  UpdateBoundsAndRedrawPopup();
  GetWidget()->Show();

  if (controller_->hide_on_outside_click())
    GetWidget()->SetCapture(this);
}

void AutofillPopupViewViews::InvalidateRow(size_t row) {
  SchedulePaintInRect(controller_->GetRowBounds(row));
}

void AutofillPopupViewViews::UpdateBoundsAndRedrawPopup() {
  GetWidget()->SetBounds(controller_->popup_bounds());
  SchedulePaint();
}

void AutofillPopupViewViews::HideInternal() {
  observing_widget_->RemoveObserver(this);
}

void AutofillPopupViewViews::DrawAutofillEntry(gfx::Canvas* canvas,
                                               int index,
                                               const gfx::Rect& entry_rect) {
  if (controller_->selected_line() == index)
    canvas->FillRect(entry_rect, kHoveredBackgroundColor);

  bool is_rtl = controller_->IsRTL();
  int value_text_width = controller_->GetNameFontForRow(index).GetStringWidth(
      controller_->names()[index]);
  int value_content_x = is_rtl ?
      entry_rect.width() - value_text_width - kEndPadding : kEndPadding;

  canvas->DrawStringInt(
      controller_->names()[index],
      controller_->GetNameFontForRow(index),
      controller_->IsWarning(index) ? kWarningTextColor : kValueTextColor,
      value_content_x,
      entry_rect.y(),
      canvas->GetStringWidth(controller_->names()[index],
                             controller_->GetNameFontForRow(index)),
      entry_rect.height(),
      gfx::Canvas::TEXT_ALIGN_CENTER);

  // Use this to figure out where all the other Autofill items should be placed.
  int x_align_left = is_rtl ? kEndPadding : entry_rect.width() - kEndPadding;

  // Draw the Autofill icon, if one exists
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  int row_height = controller_->GetRowBounds(index).height();
  if (!controller_->icons()[index].empty()) {
    int icon = controller_->GetIconResourceID(controller_->icons()[index]);
    DCHECK_NE(-1, icon);
    int icon_y = entry_rect.y() + (row_height - kAutofillIconHeight) / 2;

    x_align_left += is_rtl ? 0 : -kAutofillIconWidth;

    canvas->DrawImageInt(*rb.GetImageSkiaNamed(icon), x_align_left, icon_y);

    x_align_left += is_rtl ? kAutofillIconWidth + kIconPadding : -kIconPadding;
  }

  // Draw the name text.
  if (!is_rtl) {
    x_align_left -= canvas->GetStringWidth(controller_->subtexts()[index],
                                           controller_->subtext_font());
  }

  canvas->DrawStringInt(
      controller_->subtexts()[index],
      controller_->subtext_font(),
      kItemTextColor,
      x_align_left,
      entry_rect.y(),
      canvas->GetStringWidth(controller_->subtexts()[index],
                             controller_->subtext_font()),
      entry_rect.height(),
      gfx::Canvas::TEXT_ALIGN_CENTER);
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
