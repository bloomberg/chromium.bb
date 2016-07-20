// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_POINTER_DOWN_WATCHER_DELEGATE_H_
#define ASH_COMMON_POINTER_DOWN_WATCHER_DELEGATE_H_

#include "ash/ash_export.h"

namespace views {
class PointerDownWatcher;
}

namespace ash {

// Allows different implementations of PointerDownWatcher in mus and
// non-mus ash.
class ASH_EXPORT PointerDownWatcherDelegate {
 public:
  virtual ~PointerDownWatcherDelegate() {}

  virtual void AddPointerDownWatcher(views::PointerDownWatcher* watcher) = 0;
  virtual void RemovePointerDownWatcher(views::PointerDownWatcher* watcher) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_POINTER_DOWN_WATCHER_DELEGATE_H_
