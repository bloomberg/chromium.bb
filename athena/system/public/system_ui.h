// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_PUBLIC_SYSTEM_UI_H_
#define ATHENA_SYSTEM_PUBLIC_SYSTEM_UI_H_

#include "athena/athena_export.h"
#include "base/memory/ref_counted.h"

namespace base {
class TaskRunner;
}

namespace gfx {
class ImageSkia;
}

namespace views {
class View;
}

namespace athena {

class ATHENA_EXPORT SystemUI {
 public:
  enum ColorScheme {
    COLOR_SCHEME_LIGHT,
    COLOR_SCHEME_DARK
  };

  // Creates and deletes the singleton object of the SystemUI implementation.
  static SystemUI* Create(scoped_refptr<base::TaskRunner> io_task_runner);
  static SystemUI* Get();
  static void Shutdown();

  virtual ~SystemUI() {}

  // Sets the background image.
  virtual void SetBackgroundImage(const gfx::ImageSkia& image) = 0;

  // Creates a view which displays the time, status icons, and debug
  // information.
  virtual views::View* CreateSystemInfoView(ColorScheme color_scheme) = 0;
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_PUBLIC_SYSTEM_UI_H_
