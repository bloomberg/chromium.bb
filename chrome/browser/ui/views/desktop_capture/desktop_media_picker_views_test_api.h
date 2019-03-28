// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_

#include "content/public/browser/desktop_media_id.h"

class DesktopMediaPickerViews;

namespace views {
class Checkbox;
class View;
}  // namespace views

class DesktopMediaPickerViewsTestApi {
 public:
  DesktopMediaPickerViewsTestApi();
  DesktopMediaPickerViewsTestApi(const DesktopMediaPickerViewsTestApi&) =
      delete;
  DesktopMediaPickerViewsTestApi operator=(
      const DesktopMediaPickerViewsTestApi&) = delete;
  ~DesktopMediaPickerViewsTestApi();

  void set_picker(DesktopMediaPickerViews* picker) { picker_ = picker; }

  void FocusAudioCheckbox();
  void PressMouseOnSourceAtIndex(int index, bool double_click = false);
  void SelectTabForSourceType(content::DesktopMediaID::Type source_type);
  views::Checkbox* GetAudioShareCheckbox();

  bool HasSourceAtIndex(int index) const;
  void FocusSourceAtIndex(int index);
  void DoubleTapSourceAtIndex(int index);
  int GetSelectedSourceId() const;
  views::View* GetSelectedListView();

 private:
  const views::View* GetSourceAtIndex(int index) const;
  views::View* GetSourceAtIndex(int index);

  DesktopMediaPickerViews* picker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
