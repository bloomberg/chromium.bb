// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_WINDOW_SERVICE_DELEGATE_IMPL_H_
#define ASH_WS_WINDOW_SERVICE_DELEGATE_IMPL_H_

#include <memory>

#include "services/ws/window_service_delegate.h"

namespace ash {

class WindowServiceDelegateImpl : public ws::WindowServiceDelegate {
 public:
  WindowServiceDelegateImpl();
  ~WindowServiceDelegateImpl() override;

  // ws::WindowServiceDelegate:
  std::unique_ptr<aura::Window> NewTopLevel(
      ws::TopLevelProxyWindow* top_level_proxy_window,
      aura::PropertyConverter* property_converter,
      const base::flat_map<std::string, std::vector<uint8_t>>& properties)
      override;
  void OnUnhandledKeyEvent(const ui::KeyEvent& key_event) override;
  bool StoreAndSetCursor(aura::Window* window, ui::Cursor cursor) override;
  void RunWindowMoveLoop(aura::Window* window,
                         ws::mojom::MoveLoopSource source,
                         const gfx::Point& cursor,
                         int window_component,
                         DoneCallback callback) override;
  void CancelWindowMoveLoop() override;
  void RunDragLoop(aura::Window* window,
                   const ui::OSExchangeData& data,
                   const gfx::Point& screen_location,
                   uint32_t drag_operation,
                   ui::DragDropTypes::DragEventSource source,
                   DragDropCompletedCallback callback) override;
  void CancelDragLoop(aura::Window* window) override;
  void SetWindowResizeShadow(aura::Window* window, int hit_test) override;
  void UpdateTextInputState(aura::Window* window,
                            ui::mojom::TextInputStatePtr state) override;
  void UpdateImeVisibility(aura::Window* window,
                           bool visible,
                           ui::mojom::TextInputStatePtr state) override;
  void SetModalType(aura::Window* window, ui::ModalType type) override;
  ui::SystemInputInjector* GetSystemInputInjector() override;
  ui::EventTarget* GetGlobalEventTarget() override;
  aura::Window* GetRootWindowForDisplayId(int64_t display_id) override;
  aura::Window* GetTopmostWindowAtPoint(const gfx::Point& location_in_screen,
                                        const std::set<aura::Window*>& ignores,
                                        aura::Window** real_topmost) override;
  void ConnectToImeEngine(ime::mojom::ImeEngineRequest engine_request,
                          ime::mojom::ImeEngineClientPtr client) override;

 private:
  std::unique_ptr<ui::SystemInputInjector> system_input_injector_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceDelegateImpl);
};

}  // namespace ash

#endif  // ASH_WS_WINDOW_SERVICE_DELEGATE_IMPL_H_
