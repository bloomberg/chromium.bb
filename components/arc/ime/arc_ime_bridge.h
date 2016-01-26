// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_
#define COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/arc/arc_service.h"
#include "components/arc/ime/arc_ime_ipc_host.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class InputMethod;
}

namespace arc {

class ArcBridgeService;

// This class implements ui::TextInputClient and makes ARC windows behave
// as a text input target in Chrome OS environment.
class ArcImeBridge : public ArcService,
                     public ArcImeIpcHost::Delegate,
                     public aura::EnvObserver,
                     public aura::WindowObserver,
                     public aura::client::FocusChangeObserver,
                     public ui::TextInputClient {
 public:
  explicit ArcImeBridge(ArcBridgeService* bridge_service);
  ~ArcImeBridge() override;

  // Injects the custom IPC host object for testing purpose only.
  void SetIpcHostForTesting(scoped_ptr<ArcImeIpcHost> test_ipc_host);

  // Injects the custom IME for testing purpose only.
  void SetInputMethodForTesting(ui::InputMethod* test_input_method);

  // Overridden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // Overridden from aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ArcImeIpcHost::Delegate:
  void OnTextInputTypeChanged(ui::TextInputType type) override;
  void OnCursorRectChanged(const gfx::Rect& rect) override;

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  gfx::Rect GetCaretBounds() const override;

  // Overridden from ui::TextInputClient (with default implementation):
  // TODO(kinaba): Support each of these methods to the extent possible in
  // Android input method API.
  ui::TextInputMode GetTextInputMode() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override {}
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override {}
  void EnsureCaretInRect(const gfx::Rect& rect) override {}
  bool IsEditCommandEnabled(int command_id) override;
  void SetEditCommandForNextKeyEvent(int command_id) override {}

 private:
  ui::InputMethod* GetInputMethod();

  scoped_ptr<ArcImeIpcHost> ipc_host_;
  ui::TextInputType ime_type_;
  gfx::Rect cursor_rect_;
  bool has_composition_text_;

  aura::WindowTracker observing_root_windows_;
  aura::WindowTracker arc_windows_;
  aura::WindowTracker focused_arc_window_;

  ui::InputMethod* test_input_method_;

  DISALLOW_COPY_AND_ASSIGN(ArcImeBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_
