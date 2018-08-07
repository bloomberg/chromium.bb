// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_PORT_CLASSIC_H_
#define ASH_SHELL_PORT_CLASSIC_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_port.h"
#include "base/macros.h"

namespace ash {

class PointerWatcherAdapterClassic;

// Implementation of ShellPort for classic ash/aura. See ash/README.md for more
// details.
// DEPRECATED: Being removed, since there is no longer a distinct "classic"
// config in ash. See https://crbug.com/866523
class ASH_EXPORT ShellPortClassic : public ShellPort {
 public:
  ShellPortClassic();
  ~ShellPortClassic() override;

  // ShellPort:
  void Shutdown() override;
  void AddPointerWatcher(views::PointerWatcher* watcher,
                         views::PointerWatcherEventTypes events) override;
  void RemovePointerWatcher(views::PointerWatcher* watcher) override;
  void CreatePointerWatcherAdapter() override;

 private:
  std::unique_ptr<PointerWatcherAdapterClassic> pointer_watcher_adapter_;

  DISALLOW_COPY_AND_ASSIGN(ShellPortClassic);
};

}  // namespace ash

#endif  // ASH_SHELL_PORT_CLASSIC_H_
