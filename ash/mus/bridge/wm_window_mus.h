// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_
#define ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_

#include <memory>

#include "ash/aura/wm_window_aura.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace mus {

class WmRootWindowControllerMus;

// WmWindow implementation for mus.
//
// WmWindowMus is tied to the life of the underlying aura::Window (it is stored
// as an owned property).
class WmWindowMus : public WmWindowAura {
 public:
  explicit WmWindowMus(aura::Window* window);
  // NOTE: this class is owned by the corresponding window. You shouldn't delete
  // TODO(sky): friend deleter and make private.
  ~WmWindowMus() override;

  // Returns a WmWindow for an aura::Window, creating if necessary.
  static WmWindowMus* Get(aura::Window* window) {
    return const_cast<WmWindowMus*>(
        Get(const_cast<const aura::Window*>(window)));
  }
  static const WmWindowMus* Get(const aura::Window* window);

  static WmWindowMus* Get(views::Widget* widget);

  WmRootWindowControllerMus* GetRootWindowControllerMus() {
    return const_cast<WmRootWindowControllerMus*>(
        const_cast<const WmWindowMus*>(this)->GetRootWindowControllerMus());
  }
  const WmRootWindowControllerMus* GetRootWindowControllerMus() const;

  static WmWindowMus* AsWmWindowMus(WmWindow* window) {
    return static_cast<WmWindowMus*>(window);
  }
  static const WmWindowMus* AsWmWindowMus(const WmWindow* window) {
    return static_cast<const WmWindowMus*>(window);
  }

  // Returns true if this window is considered a shell window container.
  bool IsContainer() const;

  // WmWindow:
  WmRootWindowController* GetRootWindowController() override;
  WmShell* GetShell() const override;
  void CloseWidget() override;
  void AddLimitedPreTargetHandler(ui::EventHandler* handler) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmWindowMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_
