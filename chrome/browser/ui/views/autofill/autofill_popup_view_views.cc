// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autofill_popup_view_views.h"

#include "chrome/browser/ui/views/autofill/autofill_external_delegate_views.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

namespace {
const SkColor kPopupBackground = SkColorSetARGB(0xFF, 0x00, 0x00, 0x00);
}  // namespace

AutofillPopupViewViews::AutofillPopupViewViews(
    content::WebContents* web_contents,
    AutofillExternalDelegateViews* external_delegate)
    : AutofillPopupView(web_contents, external_delegate),
      external_delegate_(external_delegate),
      web_contents_(web_contents) {
}

AutofillPopupViewViews::~AutofillPopupViewViews() {
  external_delegate_->InvalidateView();
}

void AutofillPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  // TODO(csharp): Properly draw the popup.
  canvas->DrawColor(kPopupBackground);
}

void AutofillPopupViewViews::ShowInternal() {
  if (!GetWidget()) {
    // The widget is destroyed by the corresponding NativeWidget, so we use
    // a weak pointer to hold the reference and don't have to worry about
    // deletion.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.delegate = this;
    params.can_activate = false;
    params.transparent = true;
    params.parent = web_contents_->GetView()->GetTopLevelNativeWindow();
    widget->Init(params);
    widget->SetContentsView(this);
    widget->Show();

    gfx::Rect client_area;
    web_contents_->GetContainerBounds(&client_area);
    widget->SetBounds(client_area);
  }

  ResizePopup();

  web_contents_->GetRenderViewHost()->AddKeyboardListener(this);
}

void AutofillPopupViewViews::HideInternal() {
  GetWidget()->Close();
  web_contents_->GetRenderViewHost()->RemoveKeyboardListener(this);
}

void AutofillPopupViewViews::InvalidateRow(size_t row) {
  // TODO(csharp): invalidate this row.
}

void AutofillPopupViewViews::ResizePopup() {
  gfx::Rect popup_bounds = element_bounds();
  popup_bounds.set_y(popup_bounds.y() + popup_bounds.height());

  SetBoundsRect(popup_bounds);
}
