// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_AREA_HOST_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_AREA_HOST_H_

#include "app/gfx/native_widget_types.h"

namespace views {
class View;
}  // namespace views

namespace chromeos {

// This class is an abstraction decoupling StatusAreaView from its host
// window.
class StatusAreaHost {
 public:
  // Returns native window hosting the status area.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Opens system options dialog.
  virtual void OpenSystemOptionsDialog() const = 0;

  // Indicates if the button specified should be visible at the moment.
  virtual bool IsButtonVisible(views::View* button_view) const = 0;

 protected:
  virtual ~StatusAreaHost() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_AREA_HOST_H_

