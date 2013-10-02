// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "chromeos/ime/input_method_manager.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {
namespace input_method {

class ModeIndicatorWidget;

// ModeIndicatorController is the controller of ModeIndicatiorWidget on the
// MVC model.
class ModeIndicatorController
    : public InputMethodManager::Observer {
 public:
  // This class takes the ownership of |mi_widget|.
  explicit ModeIndicatorController(ModeIndicatorWidget *mi_widget);
  virtual ~ModeIndicatorController();

  // Set cursor location, which is the base point to display this indicator.
  // Bacisally this indicator is displayed underneath the cursor.
  void SetCursorLocation(const gfx::Rect& cursor_location);

 private:
  // InputMethodManager::Observer implementation.
  virtual void InputMethodChanged(InputMethodManager* manager,
                                  bool show_message) OVERRIDE;
  virtual void InputMethodPropertyChanged(InputMethodManager* manager) OVERRIDE;

  scoped_ptr<ModeIndicatorWidget> mi_widget_;

  DISALLOW_COPY_AND_ASSIGN(ModeIndicatorController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_CONTROLLER_H_
