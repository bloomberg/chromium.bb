// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_POINTER_WATCHER_DELEGATE_MUS_H_
#define ASH_MUS_POINTER_WATCHER_DELEGATE_MUS_H_

#include "ash/pointer_watcher_delegate.h"
#include "base/macros.h"

namespace ash {

// PointerWatcher support on mus via the WindowManagerConnection.
class PointerWatcherDelegateMus : public PointerWatcherDelegate {
 public:
  PointerWatcherDelegateMus();
  ~PointerWatcherDelegateMus() override;

  // PointerWatcherDelegate:
  void AddPointerWatcher(views::PointerWatcher* watcher) override;
  void RemovePointerWatcher(views::PointerWatcher* watcher) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerWatcherDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_POINTER_WATCHER_DELEGATE_MUS_H_
