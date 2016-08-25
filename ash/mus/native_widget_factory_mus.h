// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_NATIVE_WIDGET_FACTORY_MUS_H_
#define ASH_MUS_NATIVE_WIDGET_FACTORY_MUS_H_

#include "base/macros.h"

namespace ash {
namespace mus {

class WindowManager;

// A native widget factory used for widgets in the ash system UI itself.
// For example, this creates context menu widgets for the shelf and wallpaper.
class NativeWidgetFactoryMus {
 public:
  explicit NativeWidgetFactoryMus(WindowManager* window_manager);
  ~NativeWidgetFactoryMus();

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetFactoryMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_NATIVE_WIDGET_FACTORY_MUS_H_
