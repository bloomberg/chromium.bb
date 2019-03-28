// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views_test_api.h"

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

DesktopMediaPickerViewsTestApi::DesktopMediaPickerViewsTestApi() = default;
DesktopMediaPickerViewsTestApi::~DesktopMediaPickerViewsTestApi() = default;

void DesktopMediaPickerViewsTestApi::FocusSourceAtIndex(int index) {
  GetSourceAtIndex(index)->RequestFocus();
}

void DesktopMediaPickerViewsTestApi::FocusAudioCheckbox() {
  picker_->dialog_->audio_share_checkbox_->RequestFocus();
}

void DesktopMediaPickerViewsTestApi::PressMouseOnSourceAtIndex(
    int index,
    bool double_click) {
  int flags = ui::EF_LEFT_MOUSE_BUTTON;
  if (double_click)
    flags |= ui::EF_IS_DOUBLE_CLICK;
  GetSourceAtIndex(index)->OnMousePressed(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), flags, ui::EF_LEFT_MOUSE_BUTTON));
}

void DesktopMediaPickerViewsTestApi::DoubleTapSourceAtIndex(int index) {
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
  details.set_tap_count(2);
  ui::GestureEvent double_tap(10, 10, 0, base::TimeTicks(), details);
  GetSourceAtIndex(index)->OnGestureEvent(&double_tap);
}

void DesktopMediaPickerViewsTestApi::SelectTabForSourceType(
    content::DesktopMediaID::Type source_type) {
  const auto& source_types = picker_->dialog_->source_types_;
  const auto i =
      std::find(source_types.cbegin(), source_types.cend(), source_type);
  DCHECK(i != source_types.cend());
  picker_->dialog_->pane_->SelectTabAt(std::distance(source_types.cbegin(), i));
}

int DesktopMediaPickerViewsTestApi::GetSelectedSourceId() const {
  DesktopMediaListController* controller =
      picker_->dialog_->GetSelectedController();
  base::Optional<content::DesktopMediaID> source = controller->GetSelection();
  return source.has_value() ? source.value().id : -1;
}

bool DesktopMediaPickerViewsTestApi::HasSourceAtIndex(int index) const {
  return bool{GetSourceAtIndex(index)};
}

views::View* DesktopMediaPickerViewsTestApi::GetSelectedListView() {
  return picker_->dialog_->GetSelectedController()->view_;
}

views::Checkbox* DesktopMediaPickerViewsTestApi::GetAudioShareCheckbox() {
  return picker_->dialog_->audio_share_checkbox_;
}

const views::View* DesktopMediaPickerViewsTestApi::GetSourceAtIndex(
    int index) const {
  views::View* list = picker_->dialog_->GetSelectedController()->view_;
  return (index < list->child_count()) ? list->child_at(index) : nullptr;
}

views::View* DesktopMediaPickerViewsTestApi::GetSourceAtIndex(int index) {
  views::View* list = picker_->dialog_->GetSelectedController()->view_;
  return (index < list->child_count()) ? list->child_at(index) : nullptr;
}
