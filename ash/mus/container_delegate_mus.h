// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_CONTAINER_DELEGATE_MUS_H_
#define ASH_MUS_CONTAINER_DELEGATE_MUS_H_

#include "ash/container_delegate.h"
#include "base/macros.h"

namespace ash {
namespace sysui {

// The ContainerDelegate for mus ash.
class ContainerDelegateMus : public ContainerDelegate {
 public:
  ContainerDelegateMus();
  ~ContainerDelegateMus() override;

  // ContainerDelegate:
  bool IsInMenuContainer(views::Widget* widget) override;
  bool IsInStatusContainer(views::Widget* widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContainerDelegateMus);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_MUS_CONTAINER_DELEGATE_MUS_H_
