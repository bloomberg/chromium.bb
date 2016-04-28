// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_USER_DISPLAY_MANAGER_H_
#define COMPONENTS_MUS_WS_USER_DISPLAY_MANAGER_H_

#include <set>

#include "base/atomicops.h"
#include "base/macros.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/ws/user_id.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace gfx {
class Point;
}

namespace mus {
namespace ws {

class Display;
class DisplayManager;
class WindowManagerState;

namespace test {
class UserDisplayManagerTestApi;
}

// Provides per user display state.
class UserDisplayManager : public mojom::DisplayManager {
 public:
  UserDisplayManager(ws::DisplayManager* display_manager,
                     const UserId& user_id);
  ~UserDisplayManager() override;

  void OnFrameDecorationValuesChanged(WindowManagerState* wms);

  void AddDisplayManagerBinding(
      mojo::InterfaceRequest<mojom::DisplayManager> request);

  // Called by Display prior to |display| being removed and destroyed.
  void OnWillDestroyDisplay(Display* display);

  // Called from WindowManagerState when its EventDispatcher receives a mouse
  // event.
  void OnMouseCursorLocationChanged(const gfx::Point& point);

  // Returns a read-only handle to the shared memory which contains the global
  // mouse cursor position. Each call returns a new handle.
  mojo::ScopedSharedBufferHandle GetCursorLocationMemory();

 private:
  friend class test::UserDisplayManagerTestApi;

  // Returns the WindowManagerStates for the associated user that have frame
  // decoration values.
  std::set<const WindowManagerState*> GetWindowManagerStatesForUser() const;

  void OnObserverAdded(mojom::DisplayManagerObserver* observer);

  // Calls OnDisplays() on |observer|.
  void CallOnDisplays(mojom::DisplayManagerObserver* observer);

  // Calls observer->OnDisplaysChanged() with the display for |display|.
  void CallOnDisplayChanged(WindowManagerState* wms,
                            mojom::DisplayManagerObserver* observer);

  // Overriden from mojom::DisplayManager:
  void AddObserver(mojom::DisplayManagerObserverPtr observer) override;

  ws::DisplayManager* display_manager_;

  const UserId user_id_;

  // Set to true the first time at least one Display has valid frame values.
  bool got_valid_frame_decorations_ = false;

  mojo::BindingSet<mojom::DisplayManager> display_manager_bindings_;

  // WARNING: only use these once |got_valid_frame_decorations_| is true.
  mojo::InterfacePtrSet<mojom::DisplayManagerObserver>
      display_manager_observers_;

  // Observer used for tests.
  mojom::DisplayManagerObserver* test_observer_ = nullptr;

  // The current location of the cursor. This is always kept up to date so we
  // can atomically write this to |cursor_location_memory_| once it is created.
  base::subtle::Atomic32 current_cursor_location_;

  // A handle to a shared memory buffer that is one 64 bit integer long. We
  // share this with any connection as the same user. This buffer is lazily
  // created on the first access.
  mojo::ScopedSharedBufferHandle cursor_location_handle_;

  // The one int32 in |cursor_location_handle_|. When we write to this
  // location, we must always write to it atomically. (On the other side of the
  // mojo connection, this data must be read atomically.)
  base::subtle::Atomic32* cursor_location_memory_;

  DISALLOW_COPY_AND_ASSIGN(UserDisplayManager);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_USER_DISPLAY_MANAGER_H_
