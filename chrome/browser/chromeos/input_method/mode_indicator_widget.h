// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_WIDGET_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_WIDGET_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/input_method/delayable_widget.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {
namespace input_method {

class ModeIndicatorView;

// ModeIndicatorWidget is the widget to display IME mode with bubble shape.
class ModeIndicatorWidget : public DelayableWidget {
 public:
  ModeIndicatorWidget();
  virtual ~ModeIndicatorWidget();

  // Set cursor location, which is the base point to display this indicator.
  // Bacisally this indicator is displayed underneath the cursor.
  void SetCursorLocation(const gfx::Rect& corsor_location);
  void SetLabelTextUtf8(const std::string& text_utf8);

 private:
  ModeIndicatorView* mode_view_;

  DISALLOW_COPY_AND_ASSIGN(ModeIndicatorWidget);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_WIDGET_H_
