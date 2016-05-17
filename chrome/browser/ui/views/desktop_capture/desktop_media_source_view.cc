// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"

#include "chrome/browser/media/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

using content::DesktopMediaID;

DesktopMediaSourceView::DesktopMediaSourceView(DesktopMediaListView* parent,
                                               DesktopMediaID source_id)
    : parent_(parent),
      source_id_(source_id),
      image_view_(new views::ImageView()),
      label_(new views::Label()),
      selected_(false) {
  AddChildView(image_view_);
  AddChildView(label_);
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

DesktopMediaSourceView::~DesktopMediaSourceView() {}

const char* DesktopMediaSourceView::kDesktopMediaSourceViewClassName =
    "DesktopMediaPicker_DesktopMediaSourceView";

void DesktopMediaSourceView::SetName(const base::string16& name) {
  label_->SetText(name);
}

void DesktopMediaSourceView::SetThumbnail(const gfx::ImageSkia& thumbnail) {
  image_view_->SetImage(thumbnail);
}

void DesktopMediaSourceView::SetSelected(bool selected) {
  if (selected == selected_)
    return;
  selected_ = selected;

  if (selected) {
    // Unselect all other sources.
    Views neighbours;
    parent()->GetViewsInGroup(GetGroup(), &neighbours);
    for (Views::iterator i(neighbours.begin()); i != neighbours.end(); ++i) {
      if (*i != this) {
        DCHECK_EQ((*i)->GetClassName(),
                  DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
        DesktopMediaSourceView* source_view =
            static_cast<DesktopMediaSourceView*>(*i);
        source_view->SetSelected(false);
      }
    }

    const SkColor bg_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
    set_background(views::Background::CreateSolidBackground(bg_color));

    parent_->OnSelectionChanged();
  } else {
    set_background(NULL);
  }

  SchedulePaint();
}

const char* DesktopMediaSourceView::GetClassName() const {
  return DesktopMediaSourceView::kDesktopMediaSourceViewClassName;
}

void DesktopMediaSourceView::Layout() {
  image_view_->SetBounds(DesktopMediaPickerDialogView::kThumbnailMargin,
                         DesktopMediaPickerDialogView::kThumbnailMargin,
                         DesktopMediaPickerDialogView::kThumbnailWidth,
                         DesktopMediaPickerDialogView::kThumbnailHeight);
  label_->SetBounds(DesktopMediaPickerDialogView::kThumbnailMargin,
                    DesktopMediaPickerDialogView::kThumbnailHeight +
                        DesktopMediaPickerDialogView::kThumbnailMargin,
                    DesktopMediaPickerDialogView::kThumbnailWidth,
                    DesktopMediaPickerDialogView::kLabelHeight);
}

views::View* DesktopMediaSourceView::GetSelectedViewForGroup(int group) {
  Views neighbours;
  parent()->GetViewsInGroup(group, &neighbours);
  if (neighbours.empty())
    return NULL;

  for (Views::iterator i(neighbours.begin()); i != neighbours.end(); ++i) {
    DCHECK_EQ((*i)->GetClassName(),
              DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(*i);
    if (source_view->selected_)
      return source_view;
  }
  return NULL;
}

bool DesktopMediaSourceView::IsGroupFocusTraversable() const {
  return false;
}

void DesktopMediaSourceView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (HasFocus()) {
    gfx::Rect bounds(GetLocalBounds());
    bounds.Inset(DesktopMediaPickerDialogView::kThumbnailMargin / 2,
                 DesktopMediaPickerDialogView::kThumbnailMargin / 2);
    canvas->DrawFocusRect(bounds);
  }
}

void DesktopMediaSourceView::OnFocus() {
  View::OnFocus();
  SetSelected(true);
  ScrollRectToVisible(gfx::Rect(size()));
  // We paint differently when focused.
  SchedulePaint();
}

void DesktopMediaSourceView::OnBlur() {
  View::OnBlur();
  // We paint differently when focused.
  SchedulePaint();
}

bool DesktopMediaSourceView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.GetClickCount() == 1) {
    RequestFocus();
  } else if (event.GetClickCount() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
  }
  return true;
}

void DesktopMediaSourceView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP &&
      event->details().tap_count() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
    event->SetHandled();
    return;
  }

  // Detect tap gesture using ET_GESTURE_TAP_DOWN so the view also gets focused
  // on the long tap (when the tap gesture starts).
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    RequestFocus();
    event->SetHandled();
  }
}
