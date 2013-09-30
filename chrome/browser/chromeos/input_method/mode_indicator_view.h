// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_VIEW_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace chromeos {
namespace input_method {

// ModeIndicatorView is the view to contain the label representing the IME mode.
class ModeIndicatorView : public views::View {
 public:
  ModeIndicatorView();
  virtual ~ModeIndicatorView();

  void SetLabelTextUtf8(const std::string& text_utf8);

 private:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  void Init();

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(ModeIndicatorView);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_VIEW_H_
