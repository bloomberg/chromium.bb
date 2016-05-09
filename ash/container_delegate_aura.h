// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTAINER_DELEGATE_AURA_H_
#define ASH_CONTAINER_DELEGATE_AURA_H_

#include "ash/ash_export.h"
#include "ash/container_delegate.h"
#include "base/macros.h"

namespace views {
class Widget;
}

namespace ash {

// The ash::ContainerDelegate implementation for classic ash.
class ASH_EXPORT ContainerDelegateAura : public ContainerDelegate {
 public:
  ContainerDelegateAura();
  ~ContainerDelegateAura() override;

  // ContainerDelegate:
  bool IsInMenuContainer(views::Widget* widget) override;
  bool IsInStatusContainer(views::Widget* widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContainerDelegateAura);
};

}  // namespace ash

#endif  // ASH_CONTAINER_DELEGATE_AURA_H_
