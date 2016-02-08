// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_bridge.h"

#include "base/logging.h"
#include "components/arc/ime/arc_ime_ipc_host_impl.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace arc {

namespace {

// TODO(kinaba): handling ARC windows by window names is the same temporary
// workaroud also used in arc/input/arc_input_bridge_impl.cc. We need to move
// it to more solid implementation after focus handling in components/exo is
// stabilized.
bool IsArcWindow(const aura::Window* window) {
  return window->name() == "ExoSurface";
}

bool IsArcTopLevelWindow(const aura::Window* window) {
  return window->name() == "ExoShellSurface";
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcImeBridge main implementation:

ArcImeBridge::ArcImeBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      ipc_host_(new ArcImeIpcHostImpl(this, bridge_service)),
      ime_type_(ui::TEXT_INPUT_TYPE_NONE),
      has_composition_text_(false),
      test_input_method_(nullptr) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

ArcImeBridge::~ArcImeBridge() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);

  for (aura::Window* window : arc_windows_.windows())
    window->RemoveObserver(this);
  for (aura::Window* root : observing_root_windows_.windows())
    aura::client::GetFocusClient(root)->RemoveObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
}

void ArcImeBridge::SetIpcHostForTesting(
    scoped_ptr<ArcImeIpcHost> test_ipc_host) {
  ipc_host_ = std::move(test_ipc_host);
}

void ArcImeBridge::SetInputMethodForTesting(
    ui::InputMethod* test_input_method) {
  test_input_method_ = test_input_method;
}

ui::InputMethod* ArcImeBridge::GetInputMethod() {
  if (test_input_method_)
    return test_input_method_;
  if (!focused_arc_window_.has_windows())
    return nullptr;
  return focused_arc_window_.windows().front()->GetHost()->GetInputMethod();
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EnvObserver:

void ArcImeBridge::OnWindowInitialized(aura::Window* new_window) {
  if (IsArcWindow(new_window)) {
    arc_windows_.Add(new_window);
    new_window->AddObserver(this);
  }
}

void ArcImeBridge::OnWindowAddedToRootWindow(aura::Window* window) {
  aura::Window* root = window->GetRootWindow();
  aura::client::FocusClient* focus_client = aura::client::GetFocusClient(root);
  if (focus_client && !observing_root_windows_.Contains(root)) {
    focus_client->AddObserver(this);
    observing_root_windows_.Add(root);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::client::FocusChangeObserver:

void ArcImeBridge::OnWindowFocused(aura::Window* gained_focus,
                                   aura::Window* lost_focus) {
  // The Aura focus may or may not be on sub-window of the toplevel ARC++ frame.
  // To handle all cases, judge the state by always climbing up to the toplevel.
  gained_focus = gained_focus ? gained_focus->GetToplevelWindow() : nullptr;
  lost_focus = lost_focus ? lost_focus->GetToplevelWindow() : nullptr;
  if (lost_focus == gained_focus)
    return;

  if (lost_focus && focused_arc_window_.Contains(lost_focus)) {
    ui::InputMethod* const input_method = GetInputMethod();
    if (input_method)
      input_method->DetachTextInputClient(this);
    focused_arc_window_.Remove(lost_focus);
  }

  if (gained_focus && IsArcTopLevelWindow(gained_focus)) {
    focused_arc_window_.Add(gained_focus);
    ui::InputMethod* const input_method = GetInputMethod();
    if (input_method)
      input_method->SetFocusedTextInputClient(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from arc::ArcImeIpcHost::Delegate

void ArcImeBridge::OnTextInputTypeChanged(ui::TextInputType type) {
  if (ime_type_ == type)
    return;
  ime_type_ = type;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method) {
    input_method->OnTextInputTypeChanged(this);
    if (input_method->GetTextInputClient() == this &&
        ime_type_ != ui::TEXT_INPUT_TYPE_NONE) {
      // TODO(kinaba): crbug.com/581282. This is tentative short-term solution.
      //
      // For fully correct implementation, rather than to piggyback the "show"
      // request with the input type change, we need dedicated IPCs to share the
      // virtual keyboard show/hide states between Chromium and ARC.
      input_method->ShowImeIfNeeded();
    }
  }
}

void ArcImeBridge::OnCursorRectChanged(const gfx::Rect& rect) {
  if (cursor_rect_ == rect)
    return;
  cursor_rect_ = rect;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

////////////////////////////////////////////////////////////////////////////////
// Oberridden from ui::TextInputClient:

void ArcImeBridge::SetCompositionText(
    const ui::CompositionText& composition) {
  has_composition_text_ = !composition.text.empty();
  ipc_host_->SendSetCompositionText(composition);
}

void ArcImeBridge::ConfirmCompositionText() {
  has_composition_text_ = false;
  ipc_host_->SendConfirmCompositionText();
}

void ArcImeBridge::ClearCompositionText() {
  if (has_composition_text_) {
    has_composition_text_ = false;
    ipc_host_->SendInsertText(base::string16());
  }
}

void ArcImeBridge::InsertText(const base::string16& text) {
  has_composition_text_ = false;
  ipc_host_->SendInsertText(text);
}

void ArcImeBridge::InsertChar(const ui::KeyEvent& event) {
  // Drop 0x00-0x1f (C0 controls), 0x7f (DEL), and 0x80-0x9f (C1 controls).
  // See: https://en.wikipedia.org/wiki/Unicode_control_characters
  // They are control characters and not treated as a text insertion.
  const base::char16 ch = event.GetCharacter();
  const bool is_control_char = (0x00 <= ch && ch <= 0x1f) ||
                               (0x7f <= ch && ch <= 0x9f);

  if (!is_control_char && !ui::IsSystemKeyModifier(event.flags())) {
    has_composition_text_ = false;
    ipc_host_->SendInsertText(base::string16(1, event.GetText()));
  }
}

ui::TextInputType ArcImeBridge::GetTextInputType() const {
  return ime_type_;
}

gfx::Rect ArcImeBridge::GetCaretBounds() const {
  return cursor_rect_;
}

ui::TextInputMode ArcImeBridge::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

int ArcImeBridge::GetTextInputFlags() const {
  return ui::TEXT_INPUT_FLAG_NONE;
}

bool ArcImeBridge::CanComposeInline() const {
  return true;
}

bool ArcImeBridge::GetCompositionCharacterBounds(
    uint32_t index, gfx::Rect* rect) const {
  return false;
}

bool ArcImeBridge::HasCompositionText() const {
  return has_composition_text_;
}

bool ArcImeBridge::GetTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeBridge::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeBridge::GetSelectionRange(gfx::Range* range) const {
  return false;
}

bool ArcImeBridge::SetSelectionRange(const gfx::Range& range) {
  return false;
}

bool ArcImeBridge::DeleteRange(const gfx::Range& range) {
  return false;
}

bool ArcImeBridge::GetTextFromRange(
    const gfx::Range& range, base::string16* text) const {
  return false;
}

bool ArcImeBridge::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

bool ArcImeBridge::IsEditCommandEnabled(int command_id) {
  return false;
}

}  // namespace arc
