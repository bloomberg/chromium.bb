// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_POINTER_DOWN_WATCHER_DELEGATE_MUS_H_
#define ASH_SYSUI_POINTER_DOWN_WATCHER_DELEGATE_MUS_H_

#include "ash/common/pointer_down_watcher_delegate.h"
#include "base/macros.h"

namespace ash {

// PointerDownWatcher support on mus via the WindowManagerConnection.
class PointerDownWatcherDelegateMus : public PointerDownWatcherDelegate {
 public:
  PointerDownWatcherDelegateMus();
  ~PointerDownWatcherDelegateMus() override;

  // PointerDownWatcherDelegate:
  void AddPointerDownWatcher(views::PointerDownWatcher* watcher) override;
  void RemovePointerDownWatcher(views::PointerDownWatcher* watcher) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerDownWatcherDelegateMus);
};

}  // namespace ash

#endif  // ASH_SYSUI_POINTER_DOWN_WATCHER_DELEGATE_MUS_H_
