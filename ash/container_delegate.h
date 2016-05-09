// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTAINER_DELEGATE_H_
#define ASH_CONTAINER_DELEGATE_H_

#include "ash/ash_export.h"

namespace views {
class Widget;
}

namespace ash {

// Allows access to the window container hierarchy, which differs between
// classic ash (where ash itself manages the hierarchy) and mus (where the
// window manager manages the hierarchy).
class ASH_EXPORT ContainerDelegate {
 public:
  virtual ~ContainerDelegate() {}

  virtual bool IsInMenuContainer(views::Widget* widget) = 0;
  virtual bool IsInStatusContainer(views::Widget* widget) = 0;
};

}  // namespace ash

#endif  // ASH_CONTAINER_DELEGATE_H_
